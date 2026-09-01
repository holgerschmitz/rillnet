#include <rillnet/frame.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

namespace {

using rillnet::Frame;
using rillnet::FrameFlags;
using rillnet::FrameType;
using rillnet::FrameValidationError;
using rillnet::StreamId;

[[nodiscard]] Frame make_frame(std::size_t payload_size = 3)
{
    Frame frame;
    frame.header.type = FrameType::request;
    frame.header.stream = StreamId{1};
    frame.header.payload_size = static_cast<std::uint32_t>(payload_size);
    frame.payload.resize(payload_size);
    return frame;
}

TEST(FrameTest, AcceptsValidRequestAndResponseFrames)
{
    auto request = make_frame();
    auto response = make_frame();
    response.header.type = FrameType::response;
    response.header.flags = FrameFlags::end_of_stream;

    EXPECT_EQ(rillnet::validate_frame(request), FrameValidationError::none);
    EXPECT_EQ(rillnet::validate_frame(response), FrameValidationError::none);
}

TEST(FrameTest, RejectsPayloadLengthMismatch)
{
    auto frame = make_frame();
    frame.header.payload_size += 1;

    EXPECT_EQ(rillnet::validate_frame(frame), FrameValidationError::payload_length_mismatch);
}

TEST(FrameTest, RejectsUnsupportedProtocolVersion)
{
    auto frame = make_frame();
    frame.header.version.major += 1;

    EXPECT_EQ(rillnet::validate_frame(frame), FrameValidationError::unsupported_version);
}

TEST(FrameTest, RejectsUnknownFrameType)
{
    auto frame = make_frame();
    frame.header.type = static_cast<FrameType>(255);

    EXPECT_EQ(rillnet::validate_frame(frame), FrameValidationError::unknown_frame_type);
}

TEST(FrameTest, RejectsUnknownFlagBits)
{
    auto frame = make_frame();
    frame.header.flags = static_cast<FrameFlags>(1U << 7U);

    EXPECT_EQ(rillnet::validate_frame(frame), FrameValidationError::unknown_flags);
}

TEST(FrameTest, RejectsCancelAndErrorCombination)
{
    auto frame = make_frame();
    frame.header.flags = FrameFlags::cancel | FrameFlags::error;

    EXPECT_EQ(rillnet::validate_frame(frame), FrameValidationError::invalid_flag_combination);
}

TEST(FrameTest, RejectsInvalidStreamIdentifier)
{
    auto frame = make_frame();
    frame.header.stream = StreamId{};

    EXPECT_EQ(rillnet::validate_frame(frame), FrameValidationError::invalid_stream);
}

TEST(FrameTest, AcceptsPayloadAtConfiguredMaximum)
{
    constexpr std::uint32_t maximum = 32;
    const auto frame = make_frame(maximum);

    EXPECT_EQ(rillnet::validate_frame(frame, maximum), FrameValidationError::none);
}

TEST(FrameTest, RejectsPayloadAboveConfiguredMaximum)
{
    constexpr std::uint32_t maximum = 32;
    const auto frame = make_frame(maximum + 1);

    EXPECT_EQ(rillnet::validate_frame(frame, maximum), FrameValidationError::payload_too_large);
}

TEST(FrameTest, RejectsOversizedDeclaredPayloadBeforeAllocation)
{
    constexpr std::uint32_t maximum = 32;
    auto frame = make_frame(0);
    frame.header.payload_size = maximum + 1;

    EXPECT_EQ(rillnet::validate_frame(frame, maximum), FrameValidationError::payload_too_large);
}

} // namespace