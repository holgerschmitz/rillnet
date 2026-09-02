#include <rillnet/frame_codec.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using rillnet::decode_frame_header;
using rillnet::encode_frame;
using rillnet::encode_frame_header;
using rillnet::Frame;
using rillnet::frame_header_size;
using rillnet::FrameFlags;
using rillnet::FrameType;
using rillnet::ProtocolVersion;
using rillnet::StreamId;

[[nodiscard]] Frame make_frame()
{
    Frame frame;
    frame.header.version = ProtocolVersion{1, 2};
    frame.header.type = FrameType::response;
    frame.header.flags = FrameFlags::end_of_stream | FrameFlags::error;
    frame.header.stream = StreamId{0x0102030405060708ULL};
    frame.header.payload_size = 3;
    frame.payload = {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}};
    return frame;
}

TEST(FrameCodecTest, EncodedHeaderHasFixedSize)
{
    const auto encoded = encode_frame_header(make_frame().header);
    EXPECT_EQ(encoded.size(), frame_header_size);
}

TEST(FrameCodecTest, EncodeThenDecodeRoundTrips)
{
    const auto frame = make_frame();
    const auto encoded = encode_frame_header(frame.header);

    const auto decoded = decode_frame_header(encoded);
    ASSERT_TRUE(decoded.has_value());
    // NOLINTBEGIN(bugprone-unchecked-optional-access)
    EXPECT_EQ(decoded->version, frame.header.version);
    EXPECT_EQ(decoded->type, frame.header.type);
    EXPECT_EQ(decoded->flags, frame.header.flags);
    EXPECT_EQ(decoded->stream, frame.header.stream);
    EXPECT_EQ(decoded->payload_size, frame.header.payload_size);
    // NOLINTEND(bugprone-unchecked-optional-access)
}

TEST(FrameCodecTest, EncodingUsesExplicitBigEndianByteOrder)
{
    const auto encoded = encode_frame_header(make_frame().header);

    // version.major = 1, version.minor = 2
    EXPECT_EQ(encoded[0], std::byte{0x00});
    EXPECT_EQ(encoded[1], std::byte{0x01});
    EXPECT_EQ(encoded[2], std::byte{0x00});
    EXPECT_EQ(encoded[3], std::byte{0x02});

    // stream id = 0x0102030405060708
    EXPECT_EQ(encoded[6], std::byte{0x01});
    EXPECT_EQ(encoded[7], std::byte{0x02});
    EXPECT_EQ(encoded[8], std::byte{0x03});
    EXPECT_EQ(encoded[9], std::byte{0x04});
    EXPECT_EQ(encoded[10], std::byte{0x05});
    EXPECT_EQ(encoded[11], std::byte{0x06});
    EXPECT_EQ(encoded[12], std::byte{0x07});
    EXPECT_EQ(encoded[13], std::byte{0x08});

    // payload_size = 3
    EXPECT_EQ(encoded[14], std::byte{0x00});
    EXPECT_EQ(encoded[15], std::byte{0x00});
    EXPECT_EQ(encoded[16], std::byte{0x00});
    EXPECT_EQ(encoded[17], std::byte{0x03});
}

TEST(FrameCodecTest, DecodeFailsWhenTooFewBytes)
{
    std::array<std::byte, frame_header_size - 1> too_short{};
    EXPECT_FALSE(decode_frame_header(too_short).has_value());
}

TEST(FrameCodecTest, DecodeIgnoresTrailingBytes)
{
    const auto frame = make_frame();
    auto full_frame = encode_frame(frame);
    full_frame.push_back(std::byte{0xFF});

    const auto decoded = decode_frame_header(full_frame);
    ASSERT_TRUE(decoded.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(decoded->payload_size, frame.header.payload_size);
}

TEST(FrameCodecTest, EncodeFrameConcatenatesHeaderAndPayload)
{
    const auto frame = make_frame();
    const auto encoded = encode_frame(frame);

    ASSERT_EQ(encoded.size(), frame_header_size + frame.payload.size());
    EXPECT_TRUE(std::equal(encoded.begin() + frame_header_size, encoded.end(),
                           frame.payload.begin(), frame.payload.end()));

    const auto decoded_header =
        decode_frame_header(std::span<const std::byte>(encoded.data(), frame_header_size));
    ASSERT_TRUE(decoded_header.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(decoded_header->stream, frame.header.stream);
}

} // namespace
