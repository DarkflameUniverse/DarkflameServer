#ifndef __VENDORPRICING__H__
#define __VENDORPRICING__H__

#include <cmath>
#include <cstdint>

namespace VendorPricing {
	// The most of one item a vendor sells at once, a full stack is 999
	constexpr uint32_t MaxBuyCount = 1000;

	/**
	 * Whether a count a player asked to move out of a stack is one the stack can give
	 */
	constexpr bool IsValidStackCount(const int32_t count, const uint32_t stackCount) {
		return count > 0 && static_cast<uint32_t>(count) <= stackCount;
	}

	/**
	 * Whether a count a player asked to buy is one a vendor sells
	 */
	constexpr bool IsValidBuyCount(const uint32_t count) {
		return count > 0 && count <= MaxBuyCount;
	}

	/**
	 * The price of a number of items where each item's scaled price is rounded down on its own.
	 * A price is never negative.
	 */
	inline uint64_t GetPricePerItem(const uint32_t unitValue, const float scalar, const uint32_t count) {
		const float unitPrice = std::floor(static_cast<float>(unitValue) * scalar);
		if (!(unitPrice > 0.0f)) return 0;
		return static_cast<uint64_t>(unitPrice) * count;
	}

	/**
	 * The price of a number of items where the scaled price of all of them together is rounded down.
	 * A price is never negative.
	 */
	inline uint64_t GetPriceTotal(const uint32_t unitValue, const float scalar, const uint32_t count) {
		const float price = std::floor((static_cast<float>(unitValue) * scalar) * count);
		if (!(price > 0.0f)) return 0;
		return static_cast<uint64_t>(price);
	}
}

#endif  //!__VENDORPRICING__H__
