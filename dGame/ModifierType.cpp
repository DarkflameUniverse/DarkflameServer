#include "ModifierType.h"
#include <unordered_map>

using namespace nejlika;

namespace
{

const std::unordered_map<ModifierType, std::string> colorMap = {
	{ModifierType::Health, "#750000"},
	{ModifierType::Armor, "#525252"},
	{ModifierType::Imagination, "#0077FF"},
	{ModifierType::Offensive, "#71583B"},
	{ModifierType::Defensive, "#71583B"},
	{ModifierType::Slashing, "#666666"},
	{ModifierType::Piercing, "#4f4f4f"},
	{ModifierType::Bludgeoning, "#e84646"},
	{ModifierType::Fire, "#ff0000"},
	{ModifierType::Cold, "#94d0f2"},
	{ModifierType::Lightning, "#00a2ff"},
	{ModifierType::Corruption, "#3d00ad"},
	{ModifierType::Psychic, "#4b0161"}
};

}

const std::string& nejlika::GetModifierTypeColor(ModifierType type)
{
	const auto color = colorMap.find(type);

	if (color != colorMap.end()) {
		return color->second;
	}

	static const std::string white = "#FFFFFF";

	return white;
}