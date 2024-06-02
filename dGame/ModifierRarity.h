#pragma once

#include <cstdint>
#include <string>

namespace nejlika
{

enum class ModifierRarity : uint8_t
{
	Common,
	Uncommon,
	Rare,
	Epic,
	Legendary,
	Relic
};

namespace ModifierRarityHelper
{
	const std::string& GetModifierRarityColor(ModifierRarity rarity);
}

}
