#include <rillnet/status_code.hpp>

#include <gtest/gtest.h>

namespace {

using rillnet::StatusCategory;
using rillnet::StatusCode;

TEST(StatusCodeTest, OkIsNotAnError)
{
    EXPECT_FALSE(rillnet::is_error(StatusCode::ok));
    EXPECT_EQ(rillnet::status_category(StatusCode::ok), StatusCategory::ok);
}

TEST(StatusCodeTest, NonOkCodesAreErrors)
{
    EXPECT_TRUE(rillnet::is_error(StatusCode::transport_error));
    EXPECT_TRUE(rillnet::is_error(StatusCode::cancelled));
}

TEST(StatusCodeTest, CategoryMatchesCodeBand)
{
    EXPECT_EQ(rillnet::status_category(StatusCode::connection_reset), StatusCategory::transport);
    EXPECT_EQ(rillnet::status_category(StatusCode::unsupported_version), StatusCategory::protocol);
    EXPECT_EQ(rillnet::status_category(StatusCode::decode_error), StatusCategory::serialization);
    EXPECT_EQ(rillnet::status_category(StatusCode::operation_error), StatusCategory::operation);
    EXPECT_EQ(rillnet::status_category(StatusCode::timeout_error), StatusCategory::timeout);
    EXPECT_EQ(rillnet::status_category(StatusCode::cancelled), StatusCategory::cancellation);
    EXPECT_EQ(rillnet::status_category(StatusCode::resource_limit_exceeded),
              StatusCategory::resource_limit);
}

TEST(StatusCodeTest, ToStringReturnsReadableName)
{
    EXPECT_EQ(rillnet::to_string(StatusCode::ok), "ok");
    EXPECT_EQ(rillnet::to_string(StatusCode::unknown_message_type), "unknown_message_type");
}

} // namespace
