#pragma once

#include <cstdint>
#include <type_traits>

namespace rillnet {

// Bit flags carried in every frame header, describing how the frame should be interpreted
// independently of its payload. Values are a closed, versioned part of the wire format: unknown
// bits received from a peer must be treated as a protocol violation rather than silently ignored.
enum class FrameFlags : std::uint8_t {
    none = 0,

    // This is the terminal frame for its stream; the sender will not emit further frames for the
    // stream afterwards. Receiving more than one terminal frame for a stream is a protocol
    // violation.
    end_of_stream = 1U << 0U,

    // Requests or acknowledges cancellation of the operation associated with the frame's stream.
    cancel = 1U << 1U,

    // The frame's payload is a structured protocol error (StatusCode) rather than an ordinary
    // application message.
    error = 1U << 2U,
};

[[nodiscard]] constexpr FrameFlags operator|(FrameFlags lhs, FrameFlags rhs) noexcept
{
    using underlying = std::underlying_type_t<FrameFlags>;
    return static_cast<FrameFlags>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
}

[[nodiscard]] constexpr FrameFlags operator&(FrameFlags lhs, FrameFlags rhs) noexcept
{
    using underlying = std::underlying_type_t<FrameFlags>;
    return static_cast<FrameFlags>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
}

[[nodiscard]] constexpr FrameFlags operator~(FrameFlags value) noexcept
{
    using underlying = std::underlying_type_t<FrameFlags>;
    return static_cast<FrameFlags>(~static_cast<underlying>(value));
}

constexpr FrameFlags &operator|=(FrameFlags &lhs, FrameFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr FrameFlags &operator&=(FrameFlags &lhs, FrameFlags rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

[[nodiscard]] constexpr bool has_flag(FrameFlags flags, FrameFlags flag) noexcept
{
    return (flags & flag) == flag;
}

} // namespace rillnet
