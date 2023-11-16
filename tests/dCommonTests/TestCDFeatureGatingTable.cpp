#include <gtest/gtest.h>
#include "CDFeatureGatingTable.h"


TEST(dCommonTests, CDFeaturingGatingComparison){
	CDFeatureGating a;
	a.major = 1;
	a.current = 10;
	a.minor = 64;

	CDFeatureGating b;
	b.major = 999;
	b.current = 999;
	b.minor = 999;
	EXPECT_TRUE(b >= a);
	EXPECT_FALSE(a >= b);

	// below
	b.major = 0;
	b.current = 10;
	b.minor = 64;
	EXPECT_FALSE(b >= a);
	EXPECT_TRUE(a >= b);

	b.major = 1;
	b.current = 9;
	b.minor = 64;
	EXPECT_FALSE(b >= a);
	EXPECT_TRUE(a >= b);

	b.major = 1;
	b.current = 10;
	b.minor = 63;
	EXPECT_FALSE(b >= a);
	EXPECT_TRUE(a >= b);

	// above
	b.major = 2;
	b.current = 10;
	b.minor = 64;
	EXPECT_TRUE(b >= a);
	EXPECT_FALSE(a >= b);

	b.major = 1;
	b.current = 11;
	b.minor = 64;
	EXPECT_TRUE(b >= a);
	EXPECT_FALSE(a >= b);

	b.major = 1;
	b.current = 10;
	b.minor = 65;
	EXPECT_TRUE(b >= a);
	EXPECT_FALSE(a >= b);
}