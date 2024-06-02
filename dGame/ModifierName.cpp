#include "ModifierName.h"

#include <magic_enum.hpp>

using namespace nejlika;

nejlika::ModifierName::ModifierName(const nlohmann::json & json)
{
	name = json["name"].get<std::string>();

	if (json.contains("type"))
	{
		type = magic_enum::enum_cast<ModifierNameType>(json["type"].get<std::string>()).value_or(ModifierNameType::Prefix);
	}
	else
	{
		type = ModifierNameType::Prefix;
	}

	if (json.contains("rarity"))
	{
		rarity = magic_enum::enum_cast<ModifierRarity>(json["rarity"].get<std::string>()).value_or(ModifierRarity::Common);
	}
	else
	{
		rarity = ModifierRarity::Common;
	}
}

nejlika::ModifierName::ModifierName(const ModifierNameTemplate& templateData) {
	name = templateData.GetName();
	type = templateData.GetType();
	rarity = templateData.GetRarity();
}

nlohmann::json nejlika::ModifierName::ToJson() const
{
	nlohmann::json json;

	json["name"] = name;
	json["type"] = magic_enum::enum_name(type);
	json["rarity"] = magic_enum::enum_name(rarity);

	return json;
}

std::string nejlika::ModifierName::GenerateHtmlString() const {
	const auto& rarityColor = ModifierRarityHelper::GetModifierRarityColor(rarity);

	std::stringstream ss;

	ss << "<font color=\"" << rarityColor << "\">" << name << "</font>";

	return ss.str();
}

std::string nejlika::ModifierName::GenerateHtmlString(const std::vector<ModifierName>& names)
{
	std::stringstream ss;

	for (const auto& name : names) {
		if (name.GetType() == ModifierNameType::Prefix && !name.name.empty()) {
			ss << name.GenerateHtmlString() << "\n";
		}
	}

	ss << "<font color=\"#D0AB62\">NAME</font>";

	for (const auto& name : names) {
		if (name.GetType() == ModifierNameType::Suffix && !name.name.empty()) {
			ss << "\n" << name.GenerateHtmlString();
		}
	}

	// Remove the last newline
	auto str = ss.str();

	if (!str.empty() && str.back() == '\n') {
		str.pop_back();
	}

	return str;
}