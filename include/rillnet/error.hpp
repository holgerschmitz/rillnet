#pragma once

#include <rillnet/status_code.hpp>

#include <cstdint>

namespace rillnet {

// Defines the protocol state affected by an error. Connection errors also terminate every active
// operation on that connection.
enum class ErrorScope : std::uint8_t {
    none,
    operation,
    connection,
};

struct ErrorDisposition {
    ErrorScope scope;
    bool send_to_peer;
    bool local_only;
};

// The single source of truth for how a locally detected status is handled. Transport and protocol
// failures cannot reliably be reported on the wire; all other failures terminate only their stream
// and are suitable for an error response once operation frames are implemented.
[[nodiscard]] constexpr ErrorDisposition error_disposition(StatusCode code) noexcept
{
    switch (status_category(code)) {
    case StatusCategory::ok:
        return {ErrorScope::none, false, false};
    case StatusCategory::transport:
    case StatusCategory::protocol:
        return {ErrorScope::connection, false, true};
    case StatusCategory::serialization:
    case StatusCategory::operation:
    case StatusCategory::timeout:
    case StatusCategory::cancellation:
    case StatusCategory::resource_limit:
        return {ErrorScope::operation, true, false};
    }
    return {ErrorScope::connection, false, true};
}

[[nodiscard]] constexpr bool terminates_connection(StatusCode code) noexcept
{
    return error_disposition(code).scope == ErrorScope::connection;
}

[[nodiscard]] constexpr bool terminates_operation(StatusCode code) noexcept
{
    return error_disposition(code).scope == ErrorScope::operation;
}

} // namespace rillnet