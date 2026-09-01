#include <rillnet/protocol_version.hpp>

#include <gtest/gtest.h>

namespace {

TEST(ProtocolVersionTest, CurrentVersionIsZeroDotOne)
{
    EXPECT_EQ(rillnet::current_protocol_version.major, 0);
    EXPECT_EQ(rillnet::current_protocol_version.minor, 1);
}

TEST(ProtocolVersionTest, EqualityComparesBothFields)
{
    EXPECT_EQ((rillnet::ProtocolVersion{1, 2}), (rillnet::ProtocolVersion{1, 2}));
    EXPECT_NE((rillnet::ProtocolVersion{1, 2}), (rillnet::ProtocolVersion{1, 3}));
    EXPECT_NE((rillnet::ProtocolVersion{1, 2}), (rillnet::ProtocolVersion{2, 2}));
}

TEST(ProtocolVersionTest, OrderingComparesMajorBeforeMinor)
{
    EXPECT_LT((rillnet::ProtocolVersion{1, 9}), (rillnet::ProtocolVersion{2, 0}));
    EXPECT_LT((rillnet::ProtocolVersion{1, 1}), (rillnet::ProtocolVersion{1, 2}));
}

} // namespace
