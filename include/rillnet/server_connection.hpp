#pragma once

#include <rillnet/codec.hpp>
#include <rillnet/frame.hpp>
#include <rillnet/frame_decoder.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/message_codec.hpp>
#include <rillnet/message_registry.hpp>
#include <rillnet/transport.hpp>
#include <rillnet/write_queue.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>

namespace rillnet {

class SessionContext {
  public:
    [[nodiscard]] StreamId stream_id() const noexcept { return stream_id_; }

  private:
    template <typename> friend class ServerConnection;

    explicit SessionContext(StreamId stream_id) noexcept : stream_id_(stream_id) {}

    StreamId stream_id_;
};

// Owns one server-side connection and dispatches request frames to registered typed handlers.
// Each handler is spawned independently, so a suspended handler never stalls the read loop or
// other requests. Responses are serialized through the connection's single WriteQueue.
template <typename CodecType = PodCodec> class ServerConnection {
  public:
    ServerConnection(boost::asio::any_io_executor executor, std::unique_ptr<Transport> transport,
                     const MessageRegistry<CodecType> &registry)
        : executor_(std::move(executor)), transport_(std::move(transport)), registry_(registry),
          write_queue_(executor_, *transport_)
    {
    }

    ServerConnection(const ServerConnection &) = delete;
    ServerConnection &operator=(const ServerConnection &) = delete;

    // Registers a handler for Request. Handler must be invocable as
    // `awaitable<Response>(SessionContext&, Request)` and replaces any handler already registered
    // for Request's wire message type.
    template <typename Request, typename Handler> void handle(Handler handler)
    {
        const auto message_type = registry_.template message_type<Request>();
        if (!message_type.has_value()) {
            return;
        }

        handlers_.insert_or_assign(
            *message_type,
            [this, handler = std::move(handler)](Frame frame) -> boost::asio::awaitable<void> {
                const auto request = decode_message<Request>(registry_, frame);
                if (!request.ok()) {
                    co_return;
                }

                SessionContext context(frame.header.stream);
                const auto response = co_await handler(context, *request.value);
                const auto encoded =
                    encode_message(registry_, response, frame.header.stream, FrameType::response);
                if (encoded.ok()) {
                    co_await write_queue_.enqueue(std::move(*encoded.frame));
                }
            });
    }

    // Drives the connection's write queue and read loop until the transport is closed or fails.
    boost::asio::awaitable<void> run()
    {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await (write_queue_.run() && read_loop());
    }

  private:
    using RequestHandler = std::function<boost::asio::awaitable<void>(Frame)>;

    boost::asio::awaitable<void> read_loop()
    {
        std::array<std::byte, 4096> buffer{};
        while (transport_->is_open()) {
            std::size_t bytes_read = 0;
            try {
                bytes_read = co_await transport_->read(buffer);
            } catch (const boost::system::system_error &) {
                break;
            }
            if (bytes_read == 0) {
                break;
            }

            for (auto &frame : decoder_.push(std::span(buffer).first(bytes_read))) {
                dispatch(std::move(frame));
            }
        }
        reading_ = false;
        close_queue_when_idle();
    }

    void dispatch(Frame frame)
    {
        if (frame.header.type != FrameType::request) {
            return;
        }

        const auto message_type = peek_message_type(frame.payload);
        if (!message_type.has_value()) {
            return;
        }
        const auto found = handlers_.find(*message_type);
        if (found == handlers_.end()) {
            return;
        }

        ++active_handlers_;
        boost::asio::co_spawn(executor_, run_handler(found->second, std::move(frame)),
                              boost::asio::detached);
    }

    boost::asio::awaitable<void> run_handler(RequestHandler handler, Frame frame)
    {
        try {
            co_await handler(std::move(frame));
        } catch (...) {
            // Handler exceptions are isolated to their operation; error frames follow in Epic 4.6.
        }
        --active_handlers_;
        close_queue_when_idle();
    }

    void close_queue_when_idle()
    {
        if (!reading_ && active_handlers_ == 0) {
            write_queue_.close();
        }
    }

    boost::asio::any_io_executor executor_;
    std::unique_ptr<Transport> transport_;
    const MessageRegistry<CodecType> &registry_;
    WriteQueue write_queue_;
    FrameDecoder decoder_;
    std::unordered_map<MessageType, RequestHandler> handlers_;
    std::size_t active_handlers_ = 0;
    bool reading_ = true;
};

} // namespace rillnet