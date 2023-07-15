#include "SpawnPatterns.h"

#include "tinyxml2.h"

std::map<int32_t, SpawnPatterns> SpawnPatterns::s_SpawnPatterns;

void SpawnPatterns::LoadSpawnPatterns(const std::string& filename) {
	tinyxml2::XMLDocument doc;
	doc.LoadFile(filename.c_str());

	auto root = doc.FirstChildElement("SpawnPatterns");

	for (auto element = root->FirstChildElement("SpawnPattern"); element != nullptr; element = element->NextSiblingElement("SpawnPattern")) {
		auto lot = element->IntAttribute("lot");

		SpawnPatterns spawnPatterns(lot);

		for (auto spawnElement = element->FirstChildElement("Spawn"); spawnElement != nullptr; spawnElement = spawnElement->NextSiblingElement("Spawn")) {
			auto rating = spawnElement->FloatAttribute("rating");
			auto chance = spawnElement->FloatAttribute("chance");

			std::vector<int32_t> spawns;

			for (auto spawn = spawnElement->FirstChildElement("SpawnLot"); spawn != nullptr; spawn = spawn->NextSiblingElement("SpawnID")) {
				auto spawnID = spawn->IntAttribute("lot");

				spawns.push_back(spawnID);
			}

			spawnPatterns.AddSpawnPatterns(rating, chance, spawns);
		}

		s_SpawnPatterns.emplace(lot, spawnPatterns);
	}
}

SpawnPatterns* SpawnPatterns::FindSpawnPatterns(int32_t lot) {
	const auto& it = s_SpawnPatterns.find(lot);

	if (it != s_SpawnPatterns.end()) {
		return &it->second;
	}

	return nullptr;
}

SpawnPatterns::SpawnPatterns(int32_t lot)
{
	this->m_Lot = lot;
}

const std::map<float, std::pair<float, std::vector<int32_t>>>& SpawnPatterns::GetSpawnPatterns() const {
	return m_SpawnPatterns;
}

void SpawnPatterns::AddSpawnPatterns(float rating, float change, std::vector<int32_t> spawns) {
	m_SpawnPatterns.emplace(rating, std::make_pair(change, spawns));
}
