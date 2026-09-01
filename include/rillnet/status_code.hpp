#pragma once

#include <cstdint>
#include <string_view>

namespace rillnet {

// The broad category a StatusCode belongs to. Determines the general handling
// rules for a code: which errors terminate a single operation versus the whole connection, and
// which are meaningful to send to the remote endpoint at all.
enum class StatusCategory : std::uint8_t {
    ok,
    transport,
    protocol,
    serialization,
    operation,
    timeout,
    cancellation,
    resource_limit,
};

// Status/error codes carried on the wire and used internally to classify the outcome of a frame,
// stream or connection. Codes are grouped into fixed 1000-wide bands per StatusCategory so that
// new codes can be added within a category without renumbering existing ones, and so a code's
// category can be recovered from its numeric value alone.
//
// This enum only defines the wire-visible codes.
enum class StatusCode : std::uint16_t {
    ok = 0,

    // TransportError: failures in the underlying transport, detected locally and never received
    // from a peer.
    transport_error = 1000,
    connection_closed = 1001,
    connection_reset = 1002,
    connection_timed_out = 1003,

    // ProtocolError: the peer violated the wire protocol itself.
    protocol_error = 2000,
    unsupported_version = 2001,
    malformed_frame = 2002,
    unknown_stream = 2003,

    // SerializationError: a payload could not be encoded or decoded.
    serialization_error = 3000,
    unknown_message_type = 3001,
    decode_error = 3002,

    // OperationError: an application handler reported a failure for a specific operation.
    operation_error = 4000,

    // TimeoutError: an operation did not complete before its deadline.
    timeout_error = 5000,

    // CancellationError: an operation was cancelled, locally or by the remote peer.
    cancelled = 6000,

    // ResourceLimitError: a configured limit (queue size, payload size, ...) was exceeded.
    resource_limit_exceeded = 7000,
};

[[nodiscard]] constexpr StatusCategory status_category(StatusCode code) noexcept
{
    const auto value = static_cast<std::uint16_t>(code);
    if (value == 0) {
        return StatusCategory::ok;
    }
    switch (value / 1000) {
    case 1:
        return StatusCategory::transport;
    case 2:
        return StatusCategory::protocol;
    case 3:
        return StatusCategory::serialization;
    case 4:
        return StatusCategory::operation;
    case 5:
        return StatusCategory::timeout;
    case 6:
        return StatusCategory::cancellation;
    default:
        return StatusCategory::resource_limit;
    }
}

[[nodiscard]] constexpr bool is_error(StatusCode code) noexcept { return code != StatusCode::ok; }

[[nodiscard]] constexpr std::string_view to_string(StatusCode code) noexcept
{
    switch (code) {
    case StatusCode::ok:
        return "ok";
    case StatusCode::transport_error:
        return "transport_error";
    case StatusCode::connection_closed:
        return "connection_closed";
    case StatusCode::connection_reset:
        return "connection_reset";
    case StatusCode::connection_timed_out:
        return "connection_timed_out";
    case StatusCode::protocol_error:
        return "protocol_error";
    case StatusCode::unsupported_version:
        return "unsupported_version";
    case StatusCode::malformed_frame:
        return "malformed_frame";
    case StatusCode::unknown_stream:
        return "unknown_stream";
    case StatusCode::serialization_error:
        return "serialization_error";
    case StatusCode::unknown_message_type:
        return "unknown_message_type";
    case StatusCode::decode_error:
        return "decode_error";
    case StatusCode::operation_error:
        return "operation_error";
    case StatusCode::timeout_error:
        return "timeout_error";
    case StatusCode::cancelled:
        return "cancelled";
    case StatusCode::resource_limit_exceeded:
        return "resource_limit_exceeded";
    }
    return "unknown_status_code";
}

} // namespace rillnet
