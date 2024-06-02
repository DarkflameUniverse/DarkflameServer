#pragma	once

#include <cstdint>
#include <string>
#include <vector>

#include "eItemType.h"
#include "ModifierNameType.h"
#include "ModifierRarity.h"
#include "ModifierTemplate.h"

#include "json.hpp"

namespace nejlika
{

class ModifierNameTemplate
{
public:
	ModifierNameTemplate(const nlohmann::json& json);

	nlohmann::json ToJson() const;

	std::vector<ModifierInstance> GenerateModifiers(int32_t level) const;

	// Getters

	const std::string& GetName() const { return name; }

	ModifierNameType GetType() const { return type; }

	const std::vector<ModifierTemplate>& GetModifiers() const { return modifiers; }

	const std::vector<eItemType>& GetItemTypes() const { return itemTypes; }

	int32_t GetMinLevel() const { return minLevel; }

	int32_t GetMaxLevel() const { return maxLevel; }

	ModifierRarity GetRarity() const { return rarity; }

	int32_t GetLOT() const { return lot; }
	
private:
	std::string name;
	int32_t lot;
	ModifierNameType type;
	std::vector<ModifierTemplate> modifiers;
	std::vector<eItemType> itemTypes;
	int32_t minLevel;
	int32_t maxLevel;
	ModifierRarity rarity;
};

}
