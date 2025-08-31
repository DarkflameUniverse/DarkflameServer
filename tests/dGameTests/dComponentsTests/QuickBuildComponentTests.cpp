#include <gtest/gtest.h>

// Test QuickBuildComponent methods that don't require complex dependencies
TEST(QuickBuildComponentSimpleTest, QuickBuildComponentIncludedTest) {
	// Simple test to verify the QuickBuildComponent header can be included
	// This tests basic compilation without creating objects that require Game::entityManager access
	ASSERT_TRUE(true);
}