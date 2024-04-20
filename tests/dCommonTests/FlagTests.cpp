#include <gtest/gtest.h>
#include "Flag.h"

enum class TestFlag : uint8_t {
	FLAG0,
	FLAG1,
	FLAG2,
	FLAG3,
	FLAG4
};

/**
 * Test bitset flags
*/
TEST(FlagTests, FlagMethodTest) {
	using enum TestFlag;

	auto flag = Flag<TestFlag>{};

	// Test setting and reading single flags, exclusively
	flag.Reset(FLAG0);
	ASSERT_TRUE(flag.HasOnly(FLAG0));
	flag.Reset(FLAG2);
	ASSERT_TRUE(flag.HasOnly(FLAG2));
	ASSERT_FALSE(flag.HasOnly(FLAG1));

	// Test setting and reading multiple flags, exclusively
	flag.Reset(FLAG3, FLAG1);
	ASSERT_FALSE(flag.Has(FLAG2));
	ASSERT_TRUE(flag.Has(FLAG3));
	ASSERT_TRUE(flag.Has(FLAG1));
	ASSERT_TRUE(flag.Has(FLAG3, FLAG1));
	ASSERT_FALSE(flag.HasOnly(FLAG3));
	ASSERT_FALSE(flag.HasOnly(FLAG1));
	ASSERT_TRUE(flag.HasOnly(FLAG3, FLAG1));

	// Test flags are being properly reset for next batch of tests
	flag.Reset(FLAG0);
	ASSERT_TRUE(flag.Has(FLAG0));
	ASSERT_TRUE(flag.HasOnly(FLAG0));
	ASSERT_FALSE(flag.Has(FLAG3));
	ASSERT_FALSE(flag.Has(FLAG3, FLAG1, FLAG2));

	// Test setting and reading single flags, non-exclusively
	flag.Set(FLAG3);
	ASSERT_TRUE(flag.Has(FLAG3));
	ASSERT_FALSE(flag.Has(FLAG1));

	// Test setting and reading multiple flags, non-exclusively
	flag.Set(FLAG2, FLAG4);
	ASSERT_TRUE(flag.Has(FLAG2, FLAG4));
	ASSERT_TRUE(flag.Has(FLAG3));
	ASSERT_FALSE(flag.Has(FLAG3, FLAG1));
	ASSERT_TRUE(flag.Has(FLAG3, FLAG2, FLAG4));
	ASSERT_FALSE(flag.Has(FLAG1));
	ASSERT_FALSE(flag.Has(FLAG1, FLAG3, FLAG2, FLAG4));

	// Test unsetting and reading multiple flags, non-exclusively
	flag.Unset(FLAG3, FLAG1);
	ASSERT_FALSE(flag.Has(FLAG3, FLAG1));
	ASSERT_FALSE(flag.Has(FLAG3));
	ASSERT_TRUE(flag.Has(FLAG2, FLAG4));
	ASSERT_FALSE(flag.Has(FLAG3, FLAG2, FLAG4));
	ASSERT_FALSE(flag.Has(FLAG1));
	ASSERT_FALSE(flag.Has(FLAG1, FLAG3, FLAG2, FLAG4));
}

