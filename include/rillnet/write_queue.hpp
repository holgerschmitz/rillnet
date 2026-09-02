#pragma once

#include <rillnet/frame.hpp>
#include <rillnet/frame_codec.hpp>
#include <rillnet/status_code.hpp>
#include <rillnet/transport.hpp>

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <cstddef>
#include <limits>
#include <string>
#include <utility>

namespace rillnet {

// The outcome of a single WriteQueue::enqueue call.
struct WriteResult {
    StatusCode status = StatusCode::ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == StatusCode::ok; }

    [[nodiscard]] static WriteResult success() noexcept { return WriteResult{}; }

    [[nodiscard]] static WriteResult failure(StatusCode status, std::string message)
    {
        return WriteResult{status, std::move(message)};
    }
};

// Provides one coordinated write path per connection. Several operations may call enqueue()
// concurrently (as coroutines interleaved on the same executor); frames are handed to the
// underlying Transport strictly in the order they were enqueued, and the queue guarantees that no
// two writes overlap on the socket.
//
// A WriteQueue must be driven by a single run() coroutine, spawned once per connection for the
// lifetime of the queue. enqueue() only ever appends to the internal FIFO and never touches the
// transport directly; run() is the sole writer.
//
// The queue has no size limit by default: enqueue() never suspends the caller waiting for
// capacity.
class WriteQueue {
  public:
    // Effectively unbounded: large enough that enqueue() never blocks on capacity in practice.
    static constexpr std::size_t unbounded_capacity = (std::numeric_limits<std::size_t>::max)();

    explicit WriteQueue(boost::asio::any_io_executor executor, Transport &transport,
                        std::size_t capacity = unbounded_capacity)
        : transport_(transport), channel_(std::move(executor), capacity)
    {
    }

    WriteQueue(const WriteQueue &) = delete;
    WriteQueue &operator=(const WriteQueue &) = delete;

    // Enqueues a frame for writing, preserving FIFO order relative to every other enqueue() call.
    // Returns once the frame has been accepted into the queue, which is not the same as having
    // been written to the transport yet. Reports resource_limit_exceeded if the queue has been
    // closed or a bounded capacity is full.
    [[nodiscard]] WriteResult try_enqueue(Frame frame)
    {
        if (!channel_.try_send(boost::system::error_code{}, std::move(frame))) {
            return WriteResult::failure(StatusCode::resource_limit_exceeded,
                                        "write queue is closed or full");
        }
        return WriteResult::success();
    }

    boost::asio::awaitable<WriteResult> enqueue(Frame frame)
    {
        boost::system::error_code error;
        co_await channel_.async_send(
            boost::system::error_code{}, std::move(frame),
            boost::asio::redirect_error(boost::asio::use_awaitable, error));

        if (error) {
            co_return WriteResult::failure(StatusCode::resource_limit_exceeded, error.message());
        }
        co_return WriteResult::success();
    }

    // Runs the write loop: repeatedly takes the next queued frame in FIFO order and writes it to
    // the transport to completion before starting the next, so writes never overlap on the socket.
    // Returns normally once close() has been called and every queued frame has drained. Returns a
    // failure if a transport write fails; the queue is left closed in that case and any frames
    // still queued are discarded.
    boost::asio::awaitable<WriteResult> run()
    {
        while (true) {
            boost::system::error_code error;
            auto frame = co_await channel_.async_receive(
                boost::asio::redirect_error(boost::asio::use_awaitable, error));

            if (error) {
                // The channel was closed and every buffered frame has already been delivered.
                co_return WriteResult::success();
            }

            try {
                const auto encoded = encode_frame(frame);
                co_await transport_.write(encoded);
            } catch (const boost::system::system_error &write_error) {
                close();
                co_return WriteResult::failure(StatusCode::transport_error, write_error.what());
            }
        }
    }

    // Stops accepting new frames. Frames already queued are still delivered to run(), which then
    // returns once they have drained. Safe to call repeatedly and from within run() itself.
    void close() noexcept { channel_.close(); }

  private:
    Transport &transport_;
    boost::asio::experimental::channel<void(boost::system::error_code, Frame)> channel_;
};

} // namespace rillnet
