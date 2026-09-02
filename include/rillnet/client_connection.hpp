#pragma once

#include <rillnet/codec.hpp>
#include <rillnet/frame.hpp>
#include <rillnet/frame_decoder.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/message_codec.hpp>
#include <rillnet/message_registry.hpp>
#include <rillnet/status_code.hpp>
#include <rillnet/stream_id_allocator.hpp>
#include <rillnet/transport.hpp>
#include <rillnet/write_queue.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <unordered_map>
#include <utility>

namespace rillnet {

// Owns one client-side connection: a Transport, the single write path required by every
// operation sharing it, and the read loop that decodes incoming frames and routes each response
// to the request awaiting it. run() must be spawned once (e.g. via co_spawn) for the lifetime of
// the connection; request() can then be called concurrently from any number of coroutines running
// on the same executor.
template <typename CodecType = PodCodec> class ClientConnection {
  public:
    ClientConnection(boost::asio::any_io_executor executor, std::unique_ptr<Transport> transport,
                     const MessageRegistry<CodecType> &registry)
        : executor_(std::move(executor)), transport_(std::move(transport)), registry_(registry),
          write_queue_(executor_, *transport_), stream_ids_(StreamInitiator::client)
    {
    }

    ClientConnection(const ClientConnection &) = delete;
    ClientConnection &operator=(const ClientConnection &) = delete;

    // Drives the connection's write queue and read loop until the transport is closed or fails.
    // Every request registered with this connection is failed with connection_closed once run()
    // returns.
    boost::asio::awaitable<void> run()
    {
        using namespace boost::asio::experimental::awaitable_operators;
        co_await (write_queue_.run() && read_loop());
    }

    // Sends a Request as a new stream and awaits the matching Response:
    //   1. allocate a stream;
    //   2. encode the request;
    //   3. send it;
    //   4. register an outstanding operation;
    //   5. await the corresponding response;
    //   6. release operation state.
    template <typename Request, typename Response>
    boost::asio::awaitable<DecodeResult<Response>> request(const Request &value)
    {
        const auto allocation = stream_ids_.allocate();
        if (!allocation.ok()) {
            co_return DecodeResult<Response>::failure(allocation.status(),
                                                      "stream identifiers exhausted");
        }
        const StreamId stream = *allocation.stream();

        const auto encoded = encode_message(registry_, value, stream);
        if (!encoded.ok()) {
            co_return DecodeResult<Response>::failure(encoded.status, encoded.message);
        }

        auto [pending, inserted] = pending_.try_emplace(stream, ResponseChannel(executor_, 1));

        const auto sent = co_await write_queue_.enqueue(std::move(*encoded.frame));
        if (!sent.ok()) {
            pending_.erase(stream);
            co_return DecodeResult<Response>::failure(sent.status, sent.message);
        }

        boost::system::error_code error;
        auto frame = co_await pending->second.async_receive(
            boost::asio::redirect_error(boost::asio::use_awaitable, error));
        pending_.erase(stream);

        if (error) {
            co_return DecodeResult<Response>::failure(
                StatusCode::connection_closed, "connection closed while awaiting a response");
        }
        co_return decode_message<Response>(registry_, frame);
    }

  private:
    using ResponseChannel =
        boost::asio::experimental::channel<void(boost::system::error_code, Frame)>;

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
                // A zero-length read means the peer closed the connection, mirroring socket EOF.
                break;
            }

            for (auto &frame : decoder_.push(std::span(buffer).first(bytes_read))) {
                dispatch(std::move(frame));
            }
        }

        for (auto &[stream, channel] : pending_) {
            channel.close();
        }
        write_queue_.close();
    }

    void dispatch(Frame frame)
    {
        if (frame.header.type != FrameType::response) {
            return;
        }

        const auto found = pending_.find(frame.header.stream);
        if (found == pending_.end()) {
            return; // response for an unknown or already-completed stream is silently dropped
        }
        found->second.try_send(boost::system::error_code{}, std::move(frame));
    }

    boost::asio::any_io_executor executor_;
    std::unique_ptr<Transport> transport_;
    const MessageRegistry<CodecType> &registry_;
    WriteQueue write_queue_;
    StreamIdAllocator stream_ids_;
    FrameDecoder decoder_;
    std::unordered_map<StreamId, ResponseChannel> pending_;
};

} // namespace rillnet
