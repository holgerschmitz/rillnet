#include <rillnet/message_codec.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using rillnet::Buffer;
using rillnet::decode_message;
using rillnet::decode_message_payload;
using rillnet::encode_message;
using rillnet::encode_message_payload;
using rillnet::Frame;
using rillnet::FrameFlags;
using rillnet::FrameType;
using rillnet::MessageRegistry;
using rillnet::MessageType;
using rillnet::peek_message_type;
using rillnet::PodCodec;
using rillnet::StatusCode;
using rillnet::StreamId;

struct StartSimulation {
    std::uint32_t id = 0;

    friend bool operator==(const StartSimulation &, const StartSimulation &) = default;
};

struct SimulationStarted {
    std::uint32_t id = 0;
};

TEST(MessageCodecTest, EncodesAndDecodesAMessageRoundTripThroughAFrame)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());
    const StartSimulation original{42};

    const auto encoded = encode_message(registry, original, StreamId{1}, FrameType::request,
                                        FrameFlags::end_of_stream);
    ASSERT_TRUE(encoded.ok());
    ASSERT_TRUE(encoded.frame.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const Frame &frame = *encoded.frame;
    EXPECT_EQ(frame.header.stream, StreamId{1});
    EXPECT_EQ(frame.header.type, FrameType::request);
    EXPECT_EQ(frame.header.flags, FrameFlags::end_of_stream);
    EXPECT_EQ(frame.header.payload_size, frame.payload.size());

    const auto decoded = decode_message<StartSimulation>(registry, frame);
    ASSERT_TRUE(decoded.ok());
    ASSERT_TRUE(decoded.value.has_value());
    EXPECT_EQ(*decoded.value, original); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(MessageCodecTest, EncodeMessageFailsForAnUnregisteredMessageType)
{
    MessageRegistry registry;

    const auto encoded = encode_message(registry, StartSimulation{1}, StreamId{1});

    EXPECT_FALSE(encoded.ok());
    EXPECT_EQ(encoded.status, StatusCode::unknown_message_type);
    EXPECT_FALSE(encoded.frame.has_value());
}

TEST(MessageCodecTest, EncodeMessageFailsWhenThePayloadExceedsTheConfiguredLimit)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());

    const auto encoded =
        encode_message(registry, StartSimulation{1}, StreamId{1}, FrameType::request,
                       FrameFlags::none, /*max_payload_size=*/1);

    EXPECT_FALSE(encoded.ok());
    EXPECT_EQ(encoded.status, StatusCode::resource_limit_exceeded);
}

TEST(MessageCodecTest, DecodeMessageFailsWhenTheFrameCarriesADifferentMessageType)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());
    ASSERT_TRUE(registry.register_message<SimulationStarted>(101).ok());
    const auto encoded = encode_message(registry, StartSimulation{1}, StreamId{1});
    ASSERT_TRUE(encoded.ok());

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto decoded = decode_message<SimulationStarted>(registry, *encoded.frame);

    EXPECT_FALSE(decoded.ok());
    EXPECT_EQ(decoded.status, StatusCode::unknown_message_type);
    EXPECT_FALSE(decoded.value.has_value());
}

TEST(MessageCodecTest, DecodeMessageFailsWhenThePayloadIsTooSmallToContainAMessageType)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());
    Frame frame;
    frame.header.stream = StreamId{1};
    frame.payload = Buffer{std::byte{1}, std::byte{2}};
    frame.header.payload_size = static_cast<std::uint32_t>(frame.payload.size());

    const auto decoded = decode_message<StartSimulation>(registry, frame);

    EXPECT_FALSE(decoded.ok());
    EXPECT_EQ(decoded.status, StatusCode::decode_error);
}

TEST(MessageCodecTest, DecodeMessagePropagatesACodecFailure)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());
    const auto encoded = encode_message(registry, StartSimulation{1}, StreamId{1});
    ASSERT_TRUE(encoded.ok());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    Frame frame = *encoded.frame;
    frame.payload.pop_back();
    frame.header.payload_size = static_cast<std::uint32_t>(frame.payload.size());

    const auto decoded = decode_message<StartSimulation>(registry, frame);

    EXPECT_FALSE(decoded.ok());
    EXPECT_EQ(decoded.status, StatusCode::decode_error);
}

TEST(MessageCodecTest, PeekMessageTypeReadsThePrefixWithoutRunningTheCodec)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());
    const auto encoded_payload = encode_message_payload(registry, StartSimulation{1});
    ASSERT_TRUE(encoded_payload.ok());

    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto wire_type = peek_message_type(*encoded_payload.payload);

    ASSERT_TRUE(wire_type.has_value());
    EXPECT_EQ(*wire_type, MessageType{100}); // NOLINT(bugprone-unchecked-optional-access)
}

TEST(MessageCodecTest, PeekMessageTypeReturnsNulloptForATooSmallPayload)
{
    const Buffer payload{std::byte{1}, std::byte{2}};

    EXPECT_FALSE(peek_message_type(payload).has_value());
}

TEST(MessageCodecTest, DecodeMessagePayloadRoundTripsWithoutAFrame)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.register_message<StartSimulation>(100).ok());
    const auto encoded_payload = encode_message_payload(registry, StartSimulation{7});
    ASSERT_TRUE(encoded_payload.ok());

    const auto decoded = decode_message_payload<StartSimulation>(
        registry, *encoded_payload.payload); // NOLINT(bugprone-unchecked-optional-access)

    ASSERT_TRUE(decoded.ok());
    ASSERT_TRUE(decoded.value.has_value());
    EXPECT_EQ(decoded.value->id, 7U); // NOLINT(bugprone-unchecked-optional-access)
}

} // namespace
