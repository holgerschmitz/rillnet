#include <rillnet/tcp_transport.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <utility>

namespace {

using boost::asio::ip::tcp;

struct ConnectedSocketPair {
    tcp::socket client;
    tcp::socket server;
};

ConnectedSocketPair connect_loopback_pair(boost::asio::io_context &context)
{
    tcp::acceptor acceptor(context, tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), 0));
    tcp::socket client(context);
    tcp::socket server(context);

    auto accept_future = boost::asio::co_spawn(
        context,
        [&acceptor, &server]() -> boost::asio::awaitable<void> {
            server = co_await acceptor.async_accept(boost::asio::use_awaitable);
        },
        boost::asio::use_future);

    auto connect_future = boost::asio::co_spawn(
        context,
        [&client, endpoint = acceptor.local_endpoint()]() -> boost::asio::awaitable<void> {
            co_await client.async_connect(endpoint, boost::asio::use_awaitable);
        },
        boost::asio::use_future);

    context.run();
    accept_future.get();
    connect_future.get();
    return {std::move(client), std::move(server)};
}

TEST(TcpTransportTest, TransfersBytesBetweenConnectedSockets)
{
    boost::asio::io_context context;
    auto [client_socket, server_socket] = connect_loopback_pair(context);
    context.restart();

    rillnet::TcpTransport client_transport(std::move(client_socket));
    rillnet::TcpTransport server_transport(std::move(server_socket));
    constexpr std::array payload{std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
    std::array<std::byte, payload.size()> received{};

    auto future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<std::size_t> {
            co_await client_transport.write(payload);
            co_return co_await server_transport.read(received);
        },
        boost::asio::use_future);

    context.run();

    EXPECT_EQ(future.get(), payload.size());
    EXPECT_EQ(received, payload);
}

TEST(TcpTransportTest, PeerShutdownIsReportedAsEndOfFile)
{
    boost::asio::io_context context;
    auto [client_socket, server_socket] = connect_loopback_pair(context);
    context.restart();

    rillnet::TcpTransport client_transport(std::move(client_socket));
    rillnet::TcpTransport server_transport(std::move(server_socket));

    auto future = boost::asio::co_spawn(
        context,
        [&]() -> boost::asio::awaitable<std::size_t> {
            client_transport.close();
            std::array<std::byte, 1> buffer{};
            co_return co_await server_transport.read(buffer);
        },
        boost::asio::use_future);

    context.run();

    EXPECT_EQ(future.get(), 0U);
    EXPECT_FALSE(server_transport.is_open());
}

TEST(TcpTransportTest, CloseIsIdempotentAndDisablesTransfer)
{
    boost::asio::io_context context;
    auto [client_socket, server_socket] = connect_loopback_pair(context);

    rillnet::TcpTransport transport(std::move(client_socket));

    EXPECT_TRUE(transport.is_open());
    transport.close();
    transport.close();
    EXPECT_FALSE(transport.is_open());

    server_socket.close();
}

} // namespace
