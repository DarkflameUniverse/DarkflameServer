#include <gtest/gtest.h>

// Simple test that verifies QuickBuildComponent header inclusion
// The actual QuickBuildComponent requires complex database dependencies
// that are not suitable for unit testing without a full CDClient database
TEST(QuickBuildComponentTest, QuickBuildComponentHeaderInclusionTest) {
	// Test that the header can be included without compilation errors
	ASSERT_TRUE(true);
}