#include <rillnet/transport.hpp>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <vector>

namespace {

class InMemoryTransport final : public rillnet::Transport {
  public:
    explicit InMemoryTransport(std::vector<std::byte> incoming) : incoming_(std::move(incoming)) {}

    boost::asio::awaitable<std::size_t> read(std::span<std::byte> buffer) override
    {
        if (!is_open()) {
            co_return 0;
        }

        const auto bytes_to_read = std::min(buffer.size(), incoming_.size() - read_offset_);
        std::copy_n(incoming_.begin() + static_cast<std::ptrdiff_t>(read_offset_), bytes_to_read,
                    buffer.begin());
        read_offset_ += bytes_to_read;
        co_return bytes_to_read;
    }

    boost::asio::awaitable<void> write(std::span<const std::byte> buffer) override
    {
        if (is_open()) {
            outgoing_.insert(outgoing_.end(), buffer.begin(), buffer.end());
        }
        co_return;
    }

    void close() noexcept override { open_ = false; }

    [[nodiscard]] bool is_open() const noexcept override { return open_; }

    [[nodiscard]] const std::vector<std::byte> &outgoing() const noexcept { return outgoing_; }

  private:
    std::vector<std::byte> incoming_;
    std::vector<std::byte> outgoing_;
    std::size_t read_offset_ = 0;
    bool open_ = true;
};

TEST(TransportTest, IsAbstractAndNonCopyable)
{
    static_assert(std::is_abstract_v<rillnet::Transport>);
    static_assert(!std::copy_constructible<rillnet::Transport>);
    static_assert(!std::is_copy_assignable_v<rillnet::Transport>);
}

TEST(TransportTest, TransfersBytesThroughTheAbstractInterface)
{
    boost::asio::io_context context;
    constexpr std::array incoming{std::byte{0x01}, std::byte{0x02}, std::byte{0x03}};
    InMemoryTransport implementation({incoming.begin(), incoming.end()});
    rillnet::Transport &transport = implementation;
    std::array<std::byte, 2> received{};
    constexpr std::array outgoing{std::byte{0x04}, std::byte{0x05}};

    auto future = boost::asio::co_spawn(
        context,
        [&transport, &received, outgoing]() -> boost::asio::awaitable<std::size_t> {
            const auto bytes_read = co_await transport.read(received);
            co_await transport.write(outgoing);
            co_return bytes_read;
        },
        boost::asio::use_future);

    context.run();

    EXPECT_EQ(future.get(), received.size());
    EXPECT_EQ(received, (std::array{std::byte{0x01}, std::byte{0x02}}));
    EXPECT_EQ(implementation.outgoing(), (std::vector{std::byte{0x04}, std::byte{0x05}}));
}

TEST(TransportTest, CloseIsIdempotent)
{
    InMemoryTransport transport({});

    EXPECT_TRUE(transport.is_open());
    transport.close();
    transport.close();
    EXPECT_FALSE(transport.is_open());
}

} // namespace