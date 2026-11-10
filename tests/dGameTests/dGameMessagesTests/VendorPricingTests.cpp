#include <gtest/gtest.h>

#include "VendorPricing.h"

TEST(VendorPricingTest, AStackGivesWhatItHolds) {
	EXPECT_TRUE(VendorPricing::IsValidStackCount(1, 1));
	EXPECT_TRUE(VendorPricing::IsValidStackCount(1, 5));
	EXPECT_TRUE(VendorPricing::IsValidStackCount(5, 5));
	EXPECT_FALSE(VendorPricing::IsValidStackCount(6, 5));
	EXPECT_FALSE(VendorPricing::IsValidStackCount(0, 5));
	EXPECT_FALSE(VendorPricing::IsValidStackCount(-1, 5));
	EXPECT_FALSE(VendorPricing::IsValidStackCount(INT32_MIN, 5));
	EXPECT_FALSE(VendorPricing::IsValidStackCount(INT32_MAX, 5));
	EXPECT_FALSE(VendorPricing::IsValidStackCount(1, 0));
}

TEST(VendorPricingTest, AVendorSellsUpToAFullStackAtOnce) {
	EXPECT_TRUE(VendorPricing::IsValidBuyCount(1));
	EXPECT_TRUE(VendorPricing::IsValidBuyCount(999));
	EXPECT_FALSE(VendorPricing::IsValidBuyCount(0));
	EXPECT_FALSE(VendorPricing::IsValidBuyCount(1000));
	// What a negative count becomes
	EXPECT_FALSE(VendorPricing::IsValidBuyCount(static_cast<uint32_t>(-1)));
}

TEST(VendorPricingTest, PricesRoundDown) {
	// 15 * 0.1 is 1.5 an item
	EXPECT_EQ(VendorPricing::GetPricePerItem(15, 0.1f, 3), 3);
	EXPECT_EQ(VendorPricing::GetPriceTotal(15, 0.1f, 3), 4);
	EXPECT_EQ(VendorPricing::GetPricePerItem(100, 1.0f, 7), 700);
	EXPECT_EQ(VendorPricing::GetPriceTotal(100, 1.0f, 7), 700);
	EXPECT_EQ(VendorPricing::GetPricePerItem(100, 0.0f, 7), 0);
	EXPECT_EQ(VendorPricing::GetPriceTotal(100, -1.0f, 7), 0);
}

TEST(VendorPricingTest, LargePricesDoNotWrap) {
	EXPECT_EQ(VendorPricing::GetPricePerItem(4000000, 1.0f, 2000), 8000000000ULL);
	EXPECT_EQ(VendorPricing::GetPriceTotal(8388608, 1.0f, 1000), 8388608000ULL);
	EXPECT_GT(VendorPricing::GetPricePerItem(UINT32_MAX, 1.0f, VendorPricing::MaxBuyCount), static_cast<uint64_t>(UINT32_MAX));
}
