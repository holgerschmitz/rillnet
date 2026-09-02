#include <rillnet/write_queue.hpp>

#include <rillnet/frame.hpp>
#include <rillnet/frame_codec.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/transport.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/system/system_error.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace {

using namespace rillnet;

// Records every frame it is asked to write, in the order write() is called. Detects overlapping
// writes by asserting no call starts while a previous one is still in progress.
class RecordingTransport final : public Transport {
  public:
    boost::asio::awaitable<std::size_t> read(std::span<std::byte> /*buffer*/) override
    {
        co_return 0;
    }

    boost::asio::awaitable<void> write(std::span<const std::byte> buffer) override
    {
        if (writing_) {
            overlap_detected_ = true;
        }
        writing_ = true;

        if (fail_after_.has_value() && writes_.size() >= *fail_after_) {
            writing_ = false;
            throw boost::system::system_error(boost::asio::error::broken_pipe);
        }

        const auto decoded = decode_frame_header(buffer.subspan(0, frame_header_size));
        EXPECT_TRUE(decoded.has_value());
        if (decoded.has_value()) {
            writes_.push_back(decoded->stream);
        }
        writing_ = false;
        co_return;
    }

    void close() noexcept override { open_ = false; }
    [[nodiscard]] bool is_open() const noexcept override { return open_; }

    void fail_after(std::size_t count) { fail_after_ = count; }

    std::vector<StreamId> writes_;
    bool overlap_detected_ = false;

  private:
    bool open_ = true;
    bool writing_ = false;
    std::optional<std::size_t> fail_after_;
};

Frame make_frame(std::uint64_t stream)
{
    Frame frame;
    frame.header.stream = StreamId{stream};
    frame.header.payload_size = 0;
    return frame;
}

boost::asio::awaitable<void> enqueue_all(rillnet::WriteQueue &queue,
                                         std::vector<std::uint64_t> streams)
{
    for (const auto stream : streams) {
        const auto result = co_await queue.enqueue(make_frame(stream));
        EXPECT_TRUE(result.ok());
    }
}

TEST(WriteQueueTest, DeliversQueuedFramesInFifoOrder)
{
    boost::asio::io_context context;
    RecordingTransport transport;
    rillnet::WriteQueue queue(context.get_executor(), transport);

    auto run_future =
        boost::asio::co_spawn(context, [&]() { return queue.run(); }, boost::asio::use_future);

    // Interleave two "operations" enqueueing frames, alternating steps so the call order is
    // deterministic: stream 1, stream 3, stream 5, stream 7.
    auto producers_future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<void> {
            co_await enqueue_all(queue, {1, 3, 5, 7});
            queue.close();
        },
        boost::asio::use_future);

    context.run();

    producers_future.get();
    auto run_result = run_future.get();
    EXPECT_TRUE(run_result.ok());

    const std::vector<StreamId> expected{StreamId{1}, StreamId{3}, StreamId{5}, StreamId{7}};
    EXPECT_EQ(transport.writes_, expected);
    EXPECT_FALSE(transport.overlap_detected_);
}

TEST(WriteQueueTest, RunReturnsAfterCloseOnceQueueDrains)
{
    boost::asio::io_context context;
    RecordingTransport transport;
    rillnet::WriteQueue queue(context.get_executor(), transport);

    auto run_future =
        boost::asio::co_spawn(context, [&]() { return queue.run(); }, boost::asio::use_future);

    auto producer_future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<void> {
            co_await queue.enqueue(make_frame(1));
            co_await queue.enqueue(make_frame(3));
            queue.close();
        },
        boost::asio::use_future);

    context.run();

    producer_future.get();
    EXPECT_TRUE(run_future.get().ok());
    EXPECT_EQ(transport.writes_.size(), 2U);
}

TEST(WriteQueueTest, TransportWriteFailureStopsRunAndReportsFailure)
{
    boost::asio::io_context context;
    RecordingTransport transport;
    transport.fail_after(0);
    rillnet::WriteQueue queue(context.get_executor(), transport);

    auto run_future =
        boost::asio::co_spawn(context, [&]() { return queue.run(); }, boost::asio::use_future);

    auto producer_future = boost::asio::co_spawn(
        context, [&]() -> boost::asio::awaitable<void> { co_await queue.enqueue(make_frame(1)); },
        boost::asio::use_future);

    context.run();

    producer_future.get();
    const auto run_result = run_future.get();
    EXPECT_FALSE(run_result.ok());
    EXPECT_EQ(run_result.status, StatusCode::transport_error);
    EXPECT_TRUE(transport.writes_.empty());
}

} // namespace
