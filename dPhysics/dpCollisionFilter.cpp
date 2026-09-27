#include "dpCollisionFilter.h"

#include <array>

namespace {
	using Table = std::array<uint32_t, dpCollisionFilter::GROUP_COUNT>;

	// Group `group` touches every group whose bit is set in `mask`, and each of those touches it back
	constexpr void Enable(Table& table, const uint32_t group, const uint32_t mask) {
		table[group - 1] |= mask;
		for (uint32_t other = 0; other < dpCollisionFilter::GROUP_COUNT; other++) {
			if (mask & (1u << other)) table[other] |= 1u << (group - 1);
		}
	}

	// The client's calls, in its order
	constexpr Table BuildTable() {
		Table table{};
		Enable(table, 1, 0x96d9);
		Enable(table, 2, 0x9400);
		Enable(table, 10, 0x1cc9);
		Enable(table, 3, 0x204d8);
		Enable(table, 12, 0x24ed8);
		Enable(table, 4, 0x1604);
		Enable(table, 13, 0x164b);
		Enable(table, 14, 0x1000);
		Enable(table, 15, 0x246d9);
		Enable(table, 24, 0xc000);
		Enable(table, 5, 0x7c5);
		Enable(table, 16, 0x4c3);
		Enable(table, 7, 0x96d5);
		Enable(table, 11, 0x9257);
		Enable(table, 8, 0x8235);
		Enable(table, 18, 0x4);
		Enable(table, 19, 0x40449);
		Enable(table, 20, 0x967d);
		Enable(table, 22, 0x451);
		Enable(table, 23, 0x401cd9);
		Enable(table, 17, 0x400280);
		Enable(table, 21, 0);
		Enable(table, 25, 0);
		return table;
	}

	constexpr Table GROUP_TABLE = BuildTable();

	// Not the same system: same group and the same system bits (and some set) never touch (0x00fb5720)
	bool DifferentSystems(const uint32_t a, const uint32_t b) {
		using namespace dpCollisionFilter;
		const bool sameSystem = (a & GROUP_BITS) == (b & GROUP_BITS)
			&& ((a & SYSTEM_BITS) != 0 || (b & SYSTEM_BITS) != 0)
			&& (a & SYSTEM_BITS) == (b & SYSTEM_BITS);
		return !sameSystem;
	}

	// A group mask touches the other's group (0x00fb5780)
	bool MaskHasGroup(const uint32_t mask, const uint32_t other) {
		using namespace dpCollisionFilter;
		const auto group = other & GROUP_BITS;
		return other == 0 || (group > 0 && group <= 32 && (mask & (1u << (group - 1))) != 0);
	}
}

bool dpCollisionFilter::GroupsCollide(const uint32_t groupA, const uint32_t groupB) {
	if (groupA == 0 || groupB == 0) return true;
	if (groupA > GROUP_COUNT || groupB > GROUP_COUNT) return false;
	return (GROUP_TABLE[groupA - 1] & (1u << (groupB - 1))) != 0;
}

bool dpCollisionFilter::ShouldCollide(uint32_t filterA, uint32_t filterB) {
	if ((filterA & PHANTOM_ONLY) || (filterB & PHANTOM_ONLY)) {
		if ((filterA & PHANTOM_ONLY) && (filterB & PHANTOM_ONLY)) return false;
		filterA &= ~PHANTOM_ONLY;
		filterB &= ~PHANTOM_ONLY;
	}

	if (filterA == 0 || filterB == 0) return true;

	const bool maskA = (filterA & GROUP_MASK) != 0;
	const bool maskB = (filterB & GROUP_MASK) != 0;
	if (!maskA && !maskB) {
		const auto groupA = filterA & GROUP_BITS;
		const auto groupB = filterB & GROUP_BITS;
		return GroupsCollide(groupA, groupB) && DifferentSystems(filterA, filterB);
	}
	if (maskA && maskB) return false;
	if (maskA) return MaskHasGroup(filterA, filterB) && DifferentSystems(filterA, filterB);
	return MaskHasGroup(filterB, filterA) && DifferentSystems(filterB, filterA);
}
