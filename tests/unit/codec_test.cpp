#include <rillnet/codec.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace {

using rillnet::Codec;
using rillnet::DecodeResult;
using rillnet::PodCodec;
using rillnet::StatusCode;

struct Point {
    std::int32_t x = 0;
    std::int32_t y = 0;

    friend bool operator==(const Point &, const Point &) = default;
};

static_assert(Codec<PodCodec, Point>);

TEST(PodCodecTest, RoundTripsATrivialValue)
{
    const Point original{3, -7};

    const auto encoded = PodCodec::encode(original);
    ASSERT_TRUE(encoded.ok());
    ASSERT_TRUE(encoded.payload.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(encoded.payload->size(), sizeof(Point));

    const DecodeResult<Point> decoded =
        PodCodec::decode<Point>(*encoded.payload); // NOLINT(bugprone-unchecked-optional-access)
    ASSERT_TRUE(decoded.ok());
    ASSERT_TRUE(decoded.value.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(*decoded.value, original);
}

TEST(PodCodecTest, FailsToDecodeAPayloadOfTheWrongSize)
{
    const rillnet::Buffer payload(sizeof(Point) - 1);

    const auto decoded = PodCodec::decode<Point>(payload);

    EXPECT_FALSE(decoded.ok());
    EXPECT_EQ(decoded.status, StatusCode::decode_error);
    EXPECT_FALSE(decoded.value.has_value());
}

TEST(EncodeResultTest, SuccessAndFailureFactoriesReportOkConsistently)
{
    const auto success = rillnet::EncodeResult::success(rillnet::Buffer{});
    EXPECT_TRUE(success.ok());

    const auto failure = rillnet::EncodeResult::failure(StatusCode::decode_error, "boom");
    EXPECT_FALSE(failure.ok());
    EXPECT_EQ(failure.message, "boom");
}

} // namespace
