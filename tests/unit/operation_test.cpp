#include <rillnet/operation.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace {

using rillnet::Operation;
using rillnet::OperationState;
using rillnet::StatusCode;
using namespace std::chrono_literals;

TEST(OperationTest, OwnsItsStreamLifecycleAndSuccessfulResult)
{
    Operation operation(rillnet::StreamId{3});

    EXPECT_EQ(operation.stream(), rillnet::StreamId{3});
    EXPECT_EQ(operation.state(), OperationState::created);
    EXPECT_FALSE(operation.result().has_value());
    EXPECT_EQ(operation.activate(), rillnet::LifecycleTransitionError::none);
    EXPECT_TRUE(operation.complete("done"));

    ASSERT_TRUE(operation.result().has_value());
    EXPECT_EQ(operation.state(), OperationState::completed);
    EXPECT_TRUE(operation.is_terminal());
    EXPECT_TRUE(operation.result()->ok());
    EXPECT_EQ(operation.result()->message(), "done");
}

TEST(OperationTest, CommitsExactlyOneTerminalResult)
{
    std::size_t completions = 0;
    Operation operation(rillnet::StreamId{3}, [&completions](const rillnet::OperationResult &result) {
        ++completions;
        EXPECT_EQ(result.status(), StatusCode::timeout_error);
    });

    ASSERT_EQ(operation.activate(), rillnet::LifecycleTransitionError::none);
    EXPECT_TRUE(operation.timeout("deadline reached"));
    EXPECT_FALSE(operation.cancel());
    EXPECT_FALSE(operation.complete());

    ASSERT_TRUE(operation.result().has_value());
    EXPECT_EQ(operation.state(), OperationState::failed);
    EXPECT_EQ(operation.result()->status(), StatusCode::timeout_error);
    EXPECT_EQ(operation.result()->message(), "deadline reached");
    EXPECT_FALSE(operation.cancellation_requested());
    EXPECT_EQ(completions, 1U);
}

TEST(OperationTest, TracksDeadlineCancellationAndTimeoutIndependently)
{
    Operation operation(rillnet::StreamId{3});
    const auto deadline = Operation::Clock::now() + 5s;

    EXPECT_TRUE(operation.set_deadline(deadline));
    ASSERT_TRUE(operation.deadline().has_value());
    EXPECT_EQ(*operation.deadline(), deadline);
    EXPECT_FALSE(operation.cancellation_requested());
    EXPECT_FALSE(operation.timed_out());

    EXPECT_TRUE(operation.request_cancellation());
    EXPECT_TRUE(operation.cancellation_requested());
    ASSERT_EQ(operation.activate(), rillnet::LifecycleTransitionError::none);
    EXPECT_TRUE(operation.timeout());

    ASSERT_TRUE(operation.result().has_value());
    EXPECT_TRUE(operation.timed_out());
    EXPECT_EQ(operation.result()->status(), StatusCode::timeout_error);
    EXPECT_FALSE(operation.set_deadline(Operation::Clock::now() + 10s));
}

TEST(OperationTest, RejectsInvalidFailureStatusAndTerminalActionsBeforeActivation)
{
    Operation operation(rillnet::StreamId{3});

    EXPECT_FALSE(operation.fail(StatusCode::ok));
    EXPECT_FALSE(operation.complete());
    EXPECT_FALSE(operation.cancel());
    EXPECT_FALSE(operation.timeout());
    EXPECT_EQ(operation.state(), OperationState::created);
    EXPECT_FALSE(operation.result().has_value());
}

} // namespace