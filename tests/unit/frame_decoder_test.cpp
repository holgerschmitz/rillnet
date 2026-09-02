#include <rillnet/frame_decoder.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

namespace {

using rillnet::encode_frame;
using rillnet::Frame;
using rillnet::FrameDecoder;
using rillnet::FrameType;
using rillnet::FrameValidationError;
using rillnet::StreamId;

[[nodiscard]] Frame make_frame(std::uint64_t stream, std::initializer_list<std::byte> payload)
{
    Frame frame;
    frame.header.type = FrameType::request;
    frame.header.stream = StreamId{stream};
    frame.header.payload_size = static_cast<std::uint32_t>(payload.size());
    frame.payload = payload;
    return frame;
}

TEST(FrameDecoderTest, EmitsFrameOnlyAfterAllOneByteFragmentsArrive)
{
    const auto expected = make_frame(1, {std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}});
    const auto encoded = encode_frame(expected);
    FrameDecoder decoder;

    for (std::size_t index = 0; index + 1 < encoded.size(); ++index) {
        EXPECT_TRUE(decoder.push(std::span{encoded}.subspan(index, 1)).empty());
        EXPECT_TRUE(decoder.has_incomplete_frame());
    }

    const auto frames = decoder.push(std::span{encoded}.subspan(encoded.size() - 1, 1));
    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0].header.stream, expected.header.stream);
    EXPECT_EQ(frames[0].payload, expected.payload);
    EXPECT_FALSE(decoder.has_incomplete_frame());
}

TEST(FrameDecoderTest, RetainsPartialPayloadUntilComplete)
{
    const auto expected = make_frame(7, {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}});
    const auto encoded = encode_frame(expected);
    FrameDecoder decoder;

    EXPECT_TRUE(decoder.push(std::span{encoded}.first(encoded.size() - 1)).empty());

    const auto frames = decoder.push(std::span{encoded}.last(1));
    ASSERT_EQ(frames.size(), 1U);
    EXPECT_EQ(frames[0].payload, expected.payload);
}

TEST(FrameDecoderTest, EmitsMultipleFramesFromOneRead)
{
    const auto first = make_frame(1, {std::byte{0x01}});
    const auto second = make_frame(3, {std::byte{0x02}, std::byte{0x03}});
    auto encoded = encode_frame(first);
    const auto encoded_second = encode_frame(second);
    encoded.insert(encoded.end(), encoded_second.begin(), encoded_second.end());
    FrameDecoder decoder;

    const auto frames = decoder.push(encoded);

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0].header.stream, first.header.stream);
    EXPECT_EQ(frames[0].payload, first.payload);
    EXPECT_EQ(frames[1].header.stream, second.header.stream);
    EXPECT_EQ(frames[1].payload, second.payload);
}

TEST(FrameDecoderTest, EmitsEmptyPayloadFrameWhenHeaderCompletes)
{
    const auto expected = make_frame(1, {});
    FrameDecoder decoder;

    const auto frames = decoder.push(encode_frame(expected));

    ASSERT_EQ(frames.size(), 1U);
    EXPECT_TRUE(frames[0].payload.empty());
}

TEST(FrameDecoderTest, RejectsOversizedPayloadBeforeReceivingPayloadBytes)
{
    auto frame = make_frame(1, {});
    frame.header.payload_size = 33;
    FrameDecoder decoder{32};

    EXPECT_TRUE(decoder.push(encode_frame(frame)).empty());
    EXPECT_EQ(decoder.error(), FrameValidationError::payload_too_large);
}

TEST(FrameDecoderTest, RejectsMalformedHeader)
{
    auto frame = make_frame(1, {});
    frame.header.stream = StreamId{};
    FrameDecoder decoder;

    EXPECT_TRUE(decoder.push(encode_frame(frame)).empty());
    EXPECT_EQ(decoder.error(), FrameValidationError::invalid_stream);
}

} // namespace