#include "ModifierRarity.h"
#include <unordered_map>

using namespace nejlika;

namespace
{

const std::unordered_map<ModifierRarity, std::string> colorMap = {
	{ModifierRarity::Common, "#FFFFFF"},
	{ModifierRarity::Uncommon, "#B5AC15"},
	{ModifierRarity::Rare, "#3EEA4A"},
	{ModifierRarity::Epic, "#2F83C1"},
	{ModifierRarity::Legendary, "#852DCA"},
	{ModifierRarity::Relic, "#00FFFF"}
};

}

const std::string& nejlika::ModifierRarityHelper::GetModifierRarityColor(ModifierRarity rarity)
{
	const auto color = colorMap.find(rarity);

	if (color != colorMap.end()) {
		return color->second;
	}

	static const std::string white = "#FFFFFF";

	return white;
}