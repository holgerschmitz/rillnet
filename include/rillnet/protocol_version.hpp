#pragma once

#include <compare>
#include <cstdint>

namespace rillnet {

// Identifies the wire protocol version used by a frame, connection or peer. Present in every
// frame header so that a version mismatch can be detected and reported before a frame's payload
// is interpreted.
struct ProtocolVersion {
    std::uint16_t major = 0;
    std::uint16_t minor = 0;

    friend constexpr auto operator<=>(const ProtocolVersion &, const ProtocolVersion &) = default;
};

// The protocol version implemented by this release of rillnet. The initial release only ever
// speaks this single version.
inline constexpr ProtocolVersion current_protocol_version{0, 1};

} // namespace rillnet
