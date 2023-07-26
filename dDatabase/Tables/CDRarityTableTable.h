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

class CDRarityTableTable : public CDTable<CDRarityTableTable> {
private:
	std::unordered_map<uint32_t, CDRarityTable> entries;

public:
	void LoadValuesFromDatabase();
	// Queries the table with a custom "where" clause
	const std::optional<CDRarityTable> Get(uint32_t predicate);
};

