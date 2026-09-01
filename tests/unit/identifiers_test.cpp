#include <rillnet/identifiers.hpp>

#include <gtest/gtest.h>

#include <unordered_set>

namespace {

TEST(IdentifiersTest, DefaultConstructedIdIsInvalid)
{
    EXPECT_FALSE(rillnet::StreamId{}.is_valid());
    EXPECT_FALSE(rillnet::MessageType{}.is_valid());
}

TEST(IdentifiersTest, ExplicitValueIsValid)
{
    EXPECT_TRUE(rillnet::StreamId{1}.is_valid());
    EXPECT_TRUE(rillnet::MessageType{1}.is_valid());
}

TEST(IdentifiersTest, EqualityComparesUnderlyingValue)
{
    EXPECT_EQ(rillnet::StreamId{7}, rillnet::StreamId{7});
    EXPECT_NE(rillnet::StreamId{7}, rillnet::StreamId{9});
}

TEST(IdentifiersTest, OrderingComparesUnderlyingValue)
{
    EXPECT_LT(rillnet::StreamId{1}, rillnet::StreamId{2});
}

TEST(IdentifiersTest, ValueReturnsUnderlyingRepresentation)
{
    EXPECT_EQ(rillnet::StreamId{42}.value(), 42U);
    EXPECT_EQ(rillnet::MessageType{100}.value(), 100U);
}

TEST(IdentifiersTest, IsUsableAsHashKey)
{
    std::unordered_set<rillnet::StreamId> streams;
    streams.insert(rillnet::StreamId{1});
    streams.insert(rillnet::StreamId{3});
    streams.insert(rillnet::StreamId{1});

    EXPECT_EQ(streams.size(), 2U);
}

TEST(IdentifiersTest, RequestIdIsAliasOfStreamId)
{
    static_assert(std::is_same_v<rillnet::RequestId, rillnet::StreamId>);
}

TEST(IdentifiersTest, OddStreamIdsAreClientAllocated)
{
    EXPECT_TRUE(rillnet::is_client_stream(rillnet::StreamId{1}));
    EXPECT_TRUE(rillnet::is_client_stream(rillnet::StreamId{3}));
    EXPECT_FALSE(rillnet::is_client_stream(rillnet::StreamId{2}));
    EXPECT_FALSE(rillnet::is_client_stream(rillnet::StreamId{}));
}

TEST(IdentifiersTest, EvenStreamIdsAreServerAllocated)
{
    EXPECT_TRUE(rillnet::is_server_stream(rillnet::StreamId{2}));
    EXPECT_TRUE(rillnet::is_server_stream(rillnet::StreamId{4}));
    EXPECT_FALSE(rillnet::is_server_stream(rillnet::StreamId{1}));
    EXPECT_FALSE(rillnet::is_server_stream(rillnet::StreamId{}));
}

} // namespace
