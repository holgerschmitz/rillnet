#pragma once

#include <rillnet/frame_codec.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace rillnet {

// Incrementally reconstructs frames from a TCP byte stream. Once a malformed header is observed,
// the decoder enters a terminal error state and accepts no more input until reset().
class FrameDecoder {
  public:
    explicit FrameDecoder(std::uint32_t max_payload_size = default_max_payload_size) noexcept
        : max_payload_size_(max_payload_size)
    {
    }

    [[nodiscard]] std::vector<Frame> push(std::span<const std::byte> bytes)
    {
        std::vector<Frame> frames;

        while (!bytes.empty() && !error_.has_value()) {
            if (!header_.has_value()) {
                const auto copied = copy_bytes(bytes, header_bytes_, header_bytes_size_);
                header_bytes_size_ += copied;
                bytes = bytes.subspan(copied);

                if (header_bytes_size_ < frame_header_size) {
                    break;
                }

                start_frame();
                if (error_.has_value()) {
                    break;
                }
            }

            const auto copied = copy_bytes(bytes, payload_, payload_bytes_size_);
            payload_bytes_size_ += copied;
            bytes = bytes.subspan(copied);

            if (payload_bytes_size_ == payload_.size()) {
                frames.push_back(Frame{*header_, std::move(payload_)});
                header_.reset();
                header_bytes_size_ = 0;
                payload_bytes_size_ = 0;
            }
        }

        return frames;
    }

    [[nodiscard]] std::optional<FrameValidationError> error() const noexcept { return error_; }

    [[nodiscard]] bool has_incomplete_frame() const noexcept
    {
        return header_.has_value() || header_bytes_size_ != 0;
    }

    void reset() noexcept
    {
        header_.reset();
        header_bytes_size_ = 0;
        payload_.clear();
        payload_bytes_size_ = 0;
        error_.reset();
    }

  private:
    [[nodiscard]] static std::size_t copy_bytes(std::span<const std::byte> source,
                                                std::span<std::byte> destination,
                                                std::size_t destination_size)
    {
        const auto copied = std::min(source.size(), destination.size() - destination_size);
        std::copy_n(source.begin(), copied, destination.begin() + destination_size);
        return copied;
    }

    void start_frame()
    {
        const auto decoded_header = decode_frame_header(header_bytes_);
        if (!decoded_header.has_value()) {
            error_ = FrameValidationError::payload_length_mismatch;
            return;
        }

        Frame candidate;
        candidate.header = *decoded_header;
        const auto validation = validate_frame(candidate, max_payload_size_);
        if (validation != FrameValidationError::none &&
            validation != FrameValidationError::payload_length_mismatch) {
            error_ = validation;
            return;
        }

        header_ = *decoded_header;
        payload_.resize(header_->payload_size);
    }

    std::uint32_t max_payload_size_;
    std::array<std::byte, frame_header_size> header_bytes_{};
    std::size_t header_bytes_size_ = 0;
    std::optional<FrameHeader> header_;
    Buffer payload_;
    std::size_t payload_bytes_size_ = 0;
    std::optional<FrameValidationError> error_;
};

} // namespace rillnet