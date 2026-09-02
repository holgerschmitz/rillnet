#pragma once

#include <rillnet/codec.hpp>
#include <rillnet/frame.hpp>
#include <rillnet/frame_codec.hpp>
#include <rillnet/frame_flags.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/message_registry.hpp>
#include <rillnet/status_code.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace rillnet {

// The number of bytes used to carry a wire MessageType ahead of the codec-encoded message body
// within a frame payload (see encode_message_payload).
inline constexpr std::size_t message_type_size = 4;

// The result of encoding a typed C++ message all the way into a protocol Frame.
struct FrameEncodeResult {
    std::optional<Frame> frame;
    StatusCode status = StatusCode::ok;
    std::string message;

    [[nodiscard]] bool ok() const noexcept { return status == StatusCode::ok; }

    [[nodiscard]] static FrameEncodeResult success(Frame frame) noexcept
    {
        return FrameEncodeResult{std::move(frame), StatusCode::ok, {}};
    }

    [[nodiscard]] static FrameEncodeResult failure(StatusCode status, std::string message)
    {
        return FrameEncodeResult{std::nullopt, status, std::move(message)};
    }
};

// Encodes a message payload as its registered wire MessageType followed by the codec-encoded
// message body:
//
//   C++ object -> codec -> [ message type | codec payload ]
//
// The message type prefix lets a receiver identify which C++ type a frame's payload decodes to
// before any codec runs (see peek_message_type / decode_message_payload).
template <typename Message, typename CodecType>
[[nodiscard]] EncodeResult encode_message_payload(const MessageRegistry<CodecType> &registry,
                                                  const Message &value)
{
    const auto wire_type = registry.template message_type<Message>();
    if (!wire_type.has_value()) {
        return EncodeResult::failure(StatusCode::unknown_message_type,
                                     "message type is not registered");
    }

    const auto encoded = CodecType::encode(value);
    if (!encoded.ok()) {
        return encoded;
    }

    Buffer payload(message_type_size + encoded.payload->size());
    detail::write_be32(payload.data(), wire_type->value());
    std::copy(encoded.payload->begin(), encoded.payload->end(),
              payload.begin() + static_cast<std::ptrdiff_t>(message_type_size));
    return EncodeResult::success(std::move(payload));
}

// Reads the wire MessageType prefixed onto a payload by encode_message_payload, without running
// any codec. Returns std::nullopt if the payload is too small to contain a message type.
[[nodiscard]] inline std::optional<MessageType> peek_message_type(const Buffer &payload) noexcept
{
    if (payload.size() < message_type_size) {
        return std::nullopt;
    }
    return MessageType{detail::read_be32(payload.data())};
}

// Decodes a payload previously produced by encode_message_payload, verifying that its wire
// message type matches Message's registration in `registry` before invoking the codec. A mismatch
// or malformed payload is reported as StatusCode::unknown_message_type / decode_error rather than
// throwing, so callers can map the outcome into the protocol error model uniformly.
template <typename Message, typename CodecType>
[[nodiscard]] DecodeResult<Message>
decode_message_payload(const MessageRegistry<CodecType> &registry, const Buffer &payload)
{
    const auto wire_type = peek_message_type(payload);
    if (!wire_type.has_value()) {
        return DecodeResult<Message>::failure(StatusCode::decode_error,
                                              "payload is too small to contain a message type");
    }

    const auto expected_type = registry.template message_type<Message>();
    if (!expected_type.has_value() || *expected_type != *wire_type) {
        return DecodeResult<Message>::failure(StatusCode::unknown_message_type,
                                              "payload message type does not match Message");
    }

    const Buffer body(payload.begin() + static_cast<std::ptrdiff_t>(message_type_size),
                      payload.end());
    return CodecType::template decode<Message>(body);
}

// Encodes a typed C++ message into a complete protocol Frame for `stream`:
//
//   C++ object -> codec -> payload bytes -> protocol frame
//
// Fails with StatusCode::unknown_message_type if Message is not registered, propagates any codec
// failure, and fails with StatusCode::resource_limit_exceeded if the encoded payload would exceed
// max_payload_size.
template <typename Message, typename CodecType>
[[nodiscard]] FrameEncodeResult
encode_message(const MessageRegistry<CodecType> &registry, const Message &value, StreamId stream,
               FrameType frame_type = FrameType::request, FrameFlags flags = FrameFlags::none,
               std::uint32_t max_payload_size = default_max_payload_size)
{
    const auto encoded = encode_message_payload(registry, value);
    if (!encoded.ok()) {
        return FrameEncodeResult::failure(encoded.status, encoded.message);
    }
    if (encoded.payload->size() > max_payload_size) {
        return FrameEncodeResult::failure(StatusCode::resource_limit_exceeded,
                                          "encoded message exceeds the maximum payload size");
    }

    FrameHeader header;
    header.type = frame_type;
    header.flags = flags;
    header.stream = stream;
    header.payload_size = static_cast<std::uint32_t>(encoded.payload->size());
    return FrameEncodeResult::success(Frame{header, std::move(*encoded.payload)});
}

// Reverses encode_message: decodes frame.payload into a Message, verifying that the frame's wire
// message type matches Message's registration in `registry`.
template <typename Message, typename CodecType>
[[nodiscard]] DecodeResult<Message> decode_message(const MessageRegistry<CodecType> &registry,
                                                   const Frame &frame)
{
    return decode_message_payload<Message>(registry, frame.payload);
}

} // namespace rillnet
