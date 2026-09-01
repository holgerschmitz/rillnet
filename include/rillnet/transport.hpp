#pragma once

#include <boost/asio/awaitable.hpp>

#include <cstddef>
#include <span>

namespace rillnet {

// Provides the byte-stream operations required by the protocol layer without exposing a
// transport-specific socket implementation.
class Transport {
  public:
    Transport() = default;
    virtual ~Transport() = default;

    Transport(const Transport &) = delete;
    Transport &operator=(const Transport &) = delete;

    // Reads up to buffer.size() bytes and returns the number of bytes transferred.
    virtual boost::asio::awaitable<std::size_t> read(std::span<std::byte> buffer) = 0;

    // Writes the complete buffer or reports an I/O error.
    virtual boost::asio::awaitable<void> write(std::span<const std::byte> buffer) = 0;

    // Cancels outstanding I/O and releases the underlying connection. Safe to call repeatedly.
    virtual void close() noexcept = 0;

    [[nodiscard]] virtual bool is_open() const noexcept = 0;
};

} // namespace rillnet