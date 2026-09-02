#pragma once

#include <rillnet/transport.hpp>

#include <algorithm>
#include <cstddef>
#include <span>
#include <vector>

namespace rillnet::testing {

// A Transport backed by an in-memory byte buffer rather than a socket, so protocol logic can be
// exercised deterministically without real networking (see docs/implementation-plan.md's
// "In-Memory Protocol Tests" strategy). Bytes handed to the constructor are delivered to read()
// in order; once they are exhausted, read() reports end-of-stream by returning 0, mirroring a
// closed socket.
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

} // namespace rillnet::testing
