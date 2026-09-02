#include <rillnet/version.hpp>

#include <gtest/gtest.h>

namespace {

TEST(VersionTest, ReportsInitialMajorVersion) { EXPECT_EQ(rillnet::version_major(), 0); }

} // namespace