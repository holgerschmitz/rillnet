#pragma once

#include <rillnet/status_code.hpp>
#include <rillnet/tcp_transport.hpp>
#include <rillnet/transport.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace rillnet {

// How long to wait for a TCP connection to be established before reporting
// StatusCode::connection_timed_out.
inline constexpr std::chrono::milliseconds default_connect_timeout{10000};

// The outcome of TcpClient::connect. Exactly one of transport (success) or status/message
// (failure) is meaningful; a failure never leaves a socket behind.
struct ConnectResult {
    std::unique_ptr<Transport> transport;
    StatusCode status = StatusCode::ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == StatusCode::ok; }

    [[nodiscard]] static ConnectResult success(std::unique_ptr<Transport> transport) noexcept
    {
        return ConnectResult{std::move(transport), StatusCode::ok, {}};
    }

    [[nodiscard]] static ConnectResult failure(StatusCode status, std::string message)
    {
        return ConnectResult{nullptr, status, std::move(message)};
    }
};

namespace detail {

// Maps a locally detected Boost.Asio failure onto the transport-category StatusCode that best
// describes it; resolution and connect failures that do not match a more specific code fall back
// to the generic transport_error.
[[nodiscard]] inline StatusCode
classify_connect_error(const boost::system::error_code &error) noexcept
{
    if (error == boost::asio::error::connection_refused ||
        error == boost::asio::error::connection_reset) {
        return StatusCode::connection_reset;
    }
    if (error == boost::asio::error::eof || error == boost::asio::error::connection_aborted) {
        return StatusCode::connection_closed;
    }
    return StatusCode::transport_error;
}

} // namespace detail

// Resolves a host/service, establishes an asynchronous TCP connection subject to a timeout, and
// reports the outcome without throwing. On success the connection's socket lifecycle is owned by
// the returned Transport.
class TcpClient {
  public:
    TcpClient() = default;

    boost::asio::awaitable<ConnectResult>
    connect(std::string host, std::uint16_t port,
            std::chrono::milliseconds timeout = default_connect_timeout)
    {
        namespace asio = boost::asio;
        using asio::experimental::awaitable_operators::operator||;
        using asio::ip::tcp;

        const auto executor = co_await asio::this_coro::executor;

        try {
            tcp::resolver resolver(executor);
            const auto endpoints =
                co_await resolver.async_resolve(host, std::to_string(port), asio::use_awaitable);

            tcp::socket socket(executor);
            asio::steady_timer timer(executor);
            timer.expires_after(timeout);

            auto outcome = co_await (asio::async_connect(socket, endpoints, asio::use_awaitable) ||
                                     timer.async_wait(asio::use_awaitable));

            if (outcome.index() == 1) {
                boost::system::error_code ignored;
                socket.close(ignored);
                co_return ConnectResult::failure(StatusCode::connection_timed_out,
                                                 "connection to " + host + " timed out");
            }

            timer.cancel();
            co_return ConnectResult::success(std::make_unique<TcpTransport>(std::move(socket)));
        } catch (const boost::system::system_error &error) {
            co_return ConnectResult::failure(detail::classify_connect_error(error.code()),
                                             error.what());
        }
    }
};

} // namespace rillnet
