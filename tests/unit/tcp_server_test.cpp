#include <rillnet/tcp_server.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <vector>

namespace {

using boost::asio::ip::tcp;
using rillnet::TcpServer;
using rillnet::Transport;

TEST(TcpServerTest, AcceptsMultipleConnectionsAndStopsCleanly)
{
    boost::asio::io_context context;
    TcpServer server(context, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    std::vector<std::unique_ptr<Transport>> connections;

    server.on_connection([&server, &connections](std::unique_ptr<Transport> connection) {
        connections.push_back(std::move(connection));
        if (connections.size() == 3) {
            server.stop();
        }
    });

    auto server_future = boost::asio::co_spawn(context, server.run(), boost::asio::use_future);
    std::vector<decltype(boost::asio::co_spawn(
        context, []() -> boost::asio::awaitable<void> { co_return; }, boost::asio::use_future))>
        client_futures;

    for (std::size_t index = 0; index < 3; ++index) {
        client_futures.push_back(boost::asio::co_spawn(
            context,
            [&context, endpoint = server.local_endpoint()]() -> boost::asio::awaitable<void> {
                tcp::socket socket(context);
                co_await socket.async_connect(endpoint, boost::asio::use_awaitable);
            },
            boost::asio::use_future));
    }

    context.run();

    server_future.get();
    for (auto &client_future : client_futures) {
        client_future.get();
    }

    ASSERT_EQ(connections.size(), 3U);
    for (const auto &connection : connections) {
        EXPECT_TRUE(connection->is_open());
    }
}

TEST(TcpServerTest, StopBeforeRunCompletesImmediately)
{
    boost::asio::io_context context;
    TcpServer server(context, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    server.stop();

    auto server_future = boost::asio::co_spawn(context, server.run(), boost::asio::use_future);
    context.run();

    server_future.get();
}

} // namespace