#include <rillnet/frame_flags.hpp>

#include <gtest/gtest.h>

namespace {

using rillnet::FrameFlags;

TEST(FrameFlagsTest, NoneHasNoFlagsSet)
{
    EXPECT_FALSE(rillnet::has_flag(FrameFlags::none, FrameFlags::end_of_stream));
    EXPECT_FALSE(rillnet::has_flag(FrameFlags::none, FrameFlags::cancel));
    EXPECT_FALSE(rillnet::has_flag(FrameFlags::none, FrameFlags::error));
}

TEST(FrameFlagsTest, OrCombinesFlags)
{
    const auto combined = FrameFlags::end_of_stream | FrameFlags::error;

    EXPECT_TRUE(rillnet::has_flag(combined, FrameFlags::end_of_stream));
    EXPECT_TRUE(rillnet::has_flag(combined, FrameFlags::error));
    EXPECT_FALSE(rillnet::has_flag(combined, FrameFlags::cancel));
}

TEST(FrameFlagsTest, OrAssignAccumulatesFlags)
{
    auto flags = FrameFlags::none;
    flags |= FrameFlags::cancel;
    flags |= FrameFlags::end_of_stream;

    EXPECT_TRUE(rillnet::has_flag(flags, FrameFlags::cancel));
    EXPECT_TRUE(rillnet::has_flag(flags, FrameFlags::end_of_stream));
}

TEST(FrameFlagsTest, AndAssignClearsUnwantedFlags)
{
    auto flags = FrameFlags::end_of_stream | FrameFlags::cancel;
    flags &= FrameFlags::cancel;

    EXPECT_FALSE(rillnet::has_flag(flags, FrameFlags::end_of_stream));
    EXPECT_TRUE(rillnet::has_flag(flags, FrameFlags::cancel));
}

TEST(FrameFlagsTest, NegationInvertsBits)
{
    const auto everything_but_cancel = ~FrameFlags::cancel;

    EXPECT_FALSE(rillnet::has_flag(everything_but_cancel, FrameFlags::cancel));
    EXPECT_TRUE(rillnet::has_flag(everything_but_cancel, FrameFlags::end_of_stream));
}

} // namespace
