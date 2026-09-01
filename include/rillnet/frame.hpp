#pragma once

#include <rillnet/frame_flags.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/protocol_version.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace rillnet {

using Buffer = std::vector<std::byte>;

// Identifies the role a frame plays in the initial request/response protocol. Values are part of
// the wire protocol; additions must be versioned and unknown values rejected by validate_frame.
enum class FrameType : std::uint8_t {
    request = 1,
    response = 2,
};

struct FrameHeader {
    ProtocolVersion version = current_protocol_version;
    FrameType type = FrameType::request;
    FrameFlags flags = FrameFlags::none;
    StreamId stream{};
    std::uint32_t payload_size = 0;
};

struct Frame {
    FrameHeader header;
    Buffer payload;
};

inline constexpr std::uint32_t default_max_payload_size = 16U * 1024U * 1024U;

enum class FrameValidationError : std::uint8_t {
    none,
    unsupported_version,
    unknown_frame_type,
    unknown_flags,
    invalid_flag_combination,
    invalid_stream,
    payload_length_mismatch,
    payload_too_large,
};

[[nodiscard]] constexpr bool is_known_frame_type(FrameType type) noexcept
{
    return type == FrameType::request || type == FrameType::response;
}

[[nodiscard]] constexpr bool has_only_known_flags(FrameFlags flags) noexcept
{
    constexpr auto known_flags = FrameFlags::end_of_stream | FrameFlags::cancel | FrameFlags::error;
    return (flags & ~known_flags) == FrameFlags::none;
}

[[nodiscard]] inline FrameValidationError
validate_frame(const Frame &frame,
               std::uint32_t max_payload_size = default_max_payload_size) noexcept
{
    if (frame.header.version != current_protocol_version) {
        return FrameValidationError::unsupported_version;
    }
    if (!is_known_frame_type(frame.header.type)) {
        return FrameValidationError::unknown_frame_type;
    }
    if (!has_only_known_flags(frame.header.flags)) {
        return FrameValidationError::unknown_flags;
    }
    if (has_flag(frame.header.flags, FrameFlags::cancel) &&
        has_flag(frame.header.flags, FrameFlags::error)) {
        return FrameValidationError::invalid_flag_combination;
    }
    if (!frame.header.stream.is_valid()) {
        return FrameValidationError::invalid_stream;
    }
    if (frame.header.payload_size > max_payload_size) {
        return FrameValidationError::payload_too_large;
    }
    if (frame.payload.size() > std::numeric_limits<std::uint32_t>::max() ||
        frame.header.payload_size != frame.payload.size()) {
        return FrameValidationError::payload_length_mismatch;
    }
    return FrameValidationError::none;
}

} // namespace rillnet