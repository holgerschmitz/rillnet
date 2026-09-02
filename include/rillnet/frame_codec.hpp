#pragma once

#include <rillnet/frame.hpp>
#include <rillnet/frame_flags.hpp>
#include <rillnet/identifiers.hpp>
#include <rillnet/protocol_version.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rillnet {

// Fixed-size, deterministic on-the-wire representation of a FrameHeader:
//
//   byte    0- 1  protocol version major (big-endian)
//   byte    2- 3  protocol version minor (big-endian)
//   byte       4  frame type
//   byte       5  frame flags
//   byte    6-13  stream id (big-endian)
//   byte   14-17  payload size (big-endian)
//
// This layout is deliberately encoded and decoded byte-by-byte rather than derived from the
// in-memory layout of FrameHeader, so that it stays stable across compilers, platforms and future
// changes to the struct (padding, member order, endianness of the host).
inline constexpr std::size_t frame_header_size = 18;

using EncodedFrameHeader = std::array<std::byte, frame_header_size>;

namespace detail {

constexpr void write_be16(std::byte *out, std::uint16_t value) noexcept
{
    out[0] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    out[1] = static_cast<std::byte>(value & 0xFFU);
}

constexpr void write_be32(std::byte *out, std::uint32_t value) noexcept
{
    out[0] = static_cast<std::byte>((value >> 24U) & 0xFFU);
    out[1] = static_cast<std::byte>((value >> 16U) & 0xFFU);
    out[2] = static_cast<std::byte>((value >> 8U) & 0xFFU);
    out[3] = static_cast<std::byte>(value & 0xFFU);
}

constexpr void write_be64(std::byte *out, std::uint64_t value) noexcept
{
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<std::byte>((value >> (56U - (8U * static_cast<unsigned>(i)))) & 0xFFU);
    }
}

[[nodiscard]] constexpr std::uint16_t read_be16(const std::byte *in) noexcept
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(in[0]) << 8U) |
                                      static_cast<std::uint16_t>(in[1]));
}

[[nodiscard]] constexpr std::uint32_t read_be32(const std::byte *in) noexcept
{
    return (static_cast<std::uint32_t>(in[0]) << 24U) | (static_cast<std::uint32_t>(in[1]) << 16U) |
           (static_cast<std::uint32_t>(in[2]) << 8U) | static_cast<std::uint32_t>(in[3]);
}

[[nodiscard]] constexpr std::uint64_t read_be64(const std::byte *in) noexcept
{
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8U) | static_cast<std::uint64_t>(in[i]);
    }
    return value;
}

} // namespace detail

// Encodes a logical FrameHeader into its deterministic wire representation. The header is encoded
// exactly as given; callers wanting a valid frame should validate it first (see validate_frame).
[[nodiscard]] constexpr EncodedFrameHeader encode_frame_header(const FrameHeader &header) noexcept
{
    EncodedFrameHeader out{};
    detail::write_be16(out.data() + 0, header.version.major);
    detail::write_be16(out.data() + 2, header.version.minor);
    out[4] = static_cast<std::byte>(header.type);
    out[5] = static_cast<std::byte>(header.flags);
    detail::write_be64(out.data() + 6, header.stream.value());
    detail::write_be32(out.data() + 14, header.payload_size);
    return out;
}

// Decodes a wire-format frame header. Returns std::nullopt only if fewer than frame_header_size
// bytes are available; it does not otherwise validate the result (see validate_frame).
[[nodiscard]] constexpr std::optional<FrameHeader>
decode_frame_header(std::span<const std::byte> bytes) noexcept
{
    if (bytes.size() < frame_header_size) {
        return std::nullopt;
    }

    FrameHeader header;
    header.version.major = detail::read_be16(bytes.data() + 0);
    header.version.minor = detail::read_be16(bytes.data() + 2);
    header.type = static_cast<FrameType>(bytes[4]);
    header.flags = static_cast<FrameFlags>(bytes[5]);
    header.stream = StreamId{detail::read_be64(bytes.data() + 6)};
    header.payload_size = detail::read_be32(bytes.data() + 14);
    return header;
}

// Encodes a full frame (header followed by payload) into a single contiguous buffer suitable for
// writing to a transport.
[[nodiscard]] inline Buffer encode_frame(const Frame &frame)
{
    Buffer out;
    out.reserve(frame_header_size + frame.payload.size());

    const auto encoded_header = encode_frame_header(frame.header);
    out.insert(out.end(), encoded_header.begin(), encoded_header.end());
    out.insert(out.end(), frame.payload.begin(), frame.payload.end());
    return out;
}

} // namespace rillnet
