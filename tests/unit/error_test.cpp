#include <rillnet/error.hpp>

#include <gtest/gtest.h>

namespace {

using rillnet::ErrorScope;
using rillnet::StatusCode;

struct ErrorPolicyCase {
    StatusCode code;
    ErrorScope scope;
    bool send_to_peer;
    bool local_only;
};

class ErrorDispositionTest : public ::testing::TestWithParam<ErrorPolicyCase> {};

TEST_P(ErrorDispositionTest, DefinesScopeAndReportingForEachStatusCategory)
{
    const auto parameter = GetParam();
    const auto disposition = rillnet::error_disposition(parameter.code);

    EXPECT_EQ(disposition.scope, parameter.scope);
    EXPECT_EQ(disposition.send_to_peer, parameter.send_to_peer);
    EXPECT_EQ(disposition.local_only, parameter.local_only);
    EXPECT_EQ(rillnet::terminates_connection(parameter.code),
              parameter.scope == ErrorScope::connection);
    EXPECT_EQ(rillnet::terminates_operation(parameter.code),
              parameter.scope == ErrorScope::operation);
}

INSTANTIATE_TEST_SUITE_P(
    StatusCategories, ErrorDispositionTest,
    ::testing::Values(
        ErrorPolicyCase{StatusCode::ok, ErrorScope::none, false, false},
        ErrorPolicyCase{StatusCode::connection_reset, ErrorScope::connection, false, true},
        ErrorPolicyCase{StatusCode::malformed_frame, ErrorScope::connection, false, true},
        ErrorPolicyCase{StatusCode::decode_error, ErrorScope::operation, true, false},
        ErrorPolicyCase{StatusCode::operation_error, ErrorScope::operation, true, false},
        ErrorPolicyCase{StatusCode::timeout_error, ErrorScope::operation, true, false},
        ErrorPolicyCase{StatusCode::cancelled, ErrorScope::operation, true, false},
        ErrorPolicyCase{StatusCode::resource_limit_exceeded, ErrorScope::operation, true, false}));

} // namespace