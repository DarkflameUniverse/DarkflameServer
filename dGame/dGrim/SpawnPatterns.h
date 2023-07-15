#pragma once

#ifndef __ESPAWNPATTERNS__H__
#define __ESPAWNPATTERNS__H__

#include <cstdint>
#include <vector>
#include <map>
#include "ItemModifierTemplate.h"

class SpawnPatterns {
public:
	SpawnPatterns(int32_t lot);

	~SpawnPatterns() = default;

	void AddSpawnPatterns(float rating, float change, std::vector<int32_t> spawns);

	const std::map<float, std::pair<float, std::vector<int32_t>>>& GetSpawnPatterns() const;

	static void LoadSpawnPatterns(const std::string& filename);

	static SpawnPatterns* FindSpawnPatterns(int32_t lot);

private:
	int32_t m_Lot;

	std::map<float, std::pair<float, std::vector<int32_t>>> m_SpawnPatterns;

	static std::map<int32_t, SpawnPatterns> s_SpawnPatterns;
};

#endif  //!__ESPAWNPATTERNS__H__