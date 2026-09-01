#pragma once

#include <rillnet/transport.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#include <cstddef>
#include <span>
#include <utility>

namespace rillnet {

// Transport implementation backed by a connected Boost.Asio TCP socket. Owns the socket for the
// lifetime of the connection; the rest of the protocol layer only sees the Transport interface.
class TcpTransport final : public Transport {
  public:
    explicit TcpTransport(boost::asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    ~TcpTransport() override { close(); }

    boost::asio::awaitable<std::size_t> read(std::span<std::byte> buffer) override
    {
        if (!is_open()) {
            co_return 0;
        }

        boost::system::error_code error;
        const auto bytes_read = co_await socket_.async_read_some(
            boost::asio::buffer(buffer.data(), buffer.size()),
            boost::asio::redirect_error(boost::asio::use_awaitable, error));

        if (error == boost::asio::error::eof) {
            close();
            co_return 0;
        }
        if (error) {
            close();
            throw boost::system::system_error(error);
        }
        co_return bytes_read;
    }

    boost::asio::awaitable<void> write(std::span<const std::byte> buffer) override
    {
        if (!is_open()) {
            throw boost::system::system_error(boost::asio::error::not_connected);
        }

        boost::system::error_code error;
        co_await boost::asio::async_write(
            socket_, boost::asio::buffer(buffer.data(), buffer.size()),
            boost::asio::redirect_error(boost::asio::use_awaitable, error));

        if (error) {
            close();
            throw boost::system::system_error(error);
        }
    }

    void close() noexcept override
    {
        if (!open_) {
            return;
        }
        open_ = false;
        boost::system::error_code ignored;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        socket_.close(ignored);
    }

    [[nodiscard]] bool is_open() const noexcept override { return open_ && socket_.is_open(); }

  private:
    boost::asio::ip::tcp::socket socket_;
    bool open_ = true;
};

} // namespace rillnet
