#pragma once

#include <rillnet/tcp_transport.hpp>
#include <rillnet/transport.hpp>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace rillnet {

// Accepts TCP connections on a supplied endpoint. Each accepted socket is transferred to the
// connection handler as a Transport, leaving the protocol layer independent of TCP details.
class TcpServer {
  public:
    using ConnectionHandler = std::function<void(std::unique_ptr<Transport>)>;

    TcpServer(boost::asio::io_context &context, const boost::asio::ip::tcp::endpoint &endpoint)
        : acceptor_(context, endpoint)
    {
    }

    ~TcpServer() { stop(); }

    TcpServer(const TcpServer &) = delete;
    TcpServer &operator=(const TcpServer &) = delete;

    void on_connection(ConnectionHandler handler) { connection_handler_ = std::move(handler); }

    // Repeatedly accepts connections until stop() is called. Transient accept failures are ignored
    // so that a server can continue accepting later connections.
    boost::asio::awaitable<void> run()
    {
        using boost::asio::ip::tcp;

        while (!stopping_) {
            boost::system::error_code error;
            tcp::socket socket = co_await acceptor_.async_accept(
                boost::asio::redirect_error(boost::asio::use_awaitable, error));

            if (error) {
                if (stopping_ || error == boost::asio::error::operation_aborted) {
                    break;
                }
                continue;
            }

            if (connection_handler_) {
                connection_handler_(std::make_unique<TcpTransport>(std::move(socket)));
            }
        }
    }

    // Stops accepting new connections and wakes a pending run() accept operation.
    void stop() noexcept
    {
        if (stopping_) {
            return;
        }

        stopping_ = true;
        boost::system::error_code ignored;
        acceptor_.cancel(ignored);
        acceptor_.close(ignored);
    }

    [[nodiscard]] boost::asio::ip::tcp::endpoint local_endpoint() const
    {
        return acceptor_.local_endpoint();
    }

  private:
    boost::asio::ip::tcp::acceptor acceptor_;
    ConnectionHandler connection_handler_;
    bool stopping_ = false;
};

} // namespace rillnet