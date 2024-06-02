#pragma once

#include <cstdint>

#include "ModifierName.h"
#include "ModifierInstance.h"

#include "json.hpp"

class Item;

namespace nejlika
{

class AdditionalItemData
{
public:
	AdditionalItemData() = default;

	AdditionalItemData(Item* item);
	
	AdditionalItemData(const nlohmann::json& json);

	nlohmann::json ToJson() const;

	void Load(const nlohmann::json& json);

	void Save(Item* item);

	void RollModifiers(Item* item, int32_t level);

	const std::vector<ModifierName>& GetModifierNames() const { return modifierNames; }
	const std::vector<ModifierInstance>& GetModifierInstances() const { return modifierInstances; }

private:
	std::vector<ModifierName> modifierNames;
	std::vector<ModifierInstance> modifierInstances;

};

}
