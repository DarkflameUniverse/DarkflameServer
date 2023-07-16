#pragma once

// Custom Classes
#include "CDTable.h"

struct CDRarityTable {
	unsigned int id;
	float randmax;
	unsigned int rarity;
	unsigned int RarityTableIndex;

	friend bool operator> (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity > c2.rarity;
	}

	friend bool operator>= (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity >= c2.rarity;
	}

	friend bool operator< (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity < c2.rarity;
	}

	friend bool operator<= (const CDRarityTable& c1, const CDRarityTable& c2) {
		return c1.rarity <= c2.rarity;
	}
};

namespace CDRarityTableTable {
private:
	std::vector<CDRarityTable> entries;

public:
	void LoadTableIntoMemory();
	// Queries the table with a custom "where" clause
	std::vector<CDRarityTable> Query(std::function<bool(CDRarityTable)> predicate);

	const std::vector<CDRarityTable>& GetEntries() const;
};

