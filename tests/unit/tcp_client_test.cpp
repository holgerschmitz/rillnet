#include <rillnet/tcp_client.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

namespace {

using boost::asio::ip::tcp;
using rillnet::ConnectResult;
using rillnet::StatusCode;
using rillnet::TcpClient;

TEST(TcpClientTest, ConnectsToAListeningServer)
{
    boost::asio::io_context context;
    tcp::acceptor acceptor(context, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    const auto port = acceptor.local_endpoint().port();

    auto accept_future = boost::asio::co_spawn(
        context,
        [&acceptor]() -> boost::asio::awaitable<void> {
            [[maybe_unused]] auto socket =
                co_await acceptor.async_accept(boost::asio::use_awaitable);
        },
        boost::asio::use_future);

    TcpClient client;
    auto connect_future = boost::asio::co_spawn(
        context,
        [&client, port]() -> boost::asio::awaitable<ConnectResult> {
            co_return co_await client.connect("127.0.0.1", port);
        },
        boost::asio::use_future);

    context.run();
    accept_future.get();
    const auto result = connect_future.get();

    ASSERT_TRUE(result.ok());
    ASSERT_NE(result.transport, nullptr);
    EXPECT_TRUE(result.transport->is_open());
}

TEST(TcpClientTest, RefusedConnectionIsReportedWithoutThrowing)
{
    boost::asio::io_context context;

    // Reserve then immediately release a port so nothing is listening on it.
    std::uint16_t port = 0;
    {
        tcp::acceptor acceptor(context,
                               tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
        port = acceptor.local_endpoint().port();
    }

    TcpClient client;
    auto connect_future = boost::asio::co_spawn(
        context,
        [&client, port]() -> boost::asio::awaitable<ConnectResult> {
            co_return co_await client.connect("127.0.0.1", port, std::chrono::milliseconds{500});
        },
        boost::asio::use_future);

    context.run();
    const auto result = connect_future.get();

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.transport, nullptr);
    // Whether an unused loopback port is reported as refused or the network silently drops the
    // attempt depends on the host's network configuration; both are a clean, non-throwing failure.
    EXPECT_TRUE(result.status == StatusCode::connection_reset ||
                result.status == StatusCode::connection_timed_out)
        << "unexpected status: " << rillnet::to_string(result.status);
    EXPECT_FALSE(result.message.empty());
}

TEST(TcpClientTest, ExpiredDeadlineIsReportedAsTimeout)
{
    boost::asio::io_context context;

    auto connect_future = boost::asio::co_spawn(
        context,
        []() -> boost::asio::awaitable<ConnectResult> {
            // 192.0.2.0/24 is reserved for documentation (RFC 5737) and is never routable, so the
            // connection attempt is guaranteed to still be pending when the short deadline elapses.
            co_return co_await TcpClient{}.connect("192.0.2.1", 80, std::chrono::milliseconds{200});
        },
        boost::asio::use_future);

    context.run();
    const auto result = connect_future.get();

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.transport, nullptr);
    EXPECT_EQ(result.status, StatusCode::connection_timed_out);
}

} // namespace
