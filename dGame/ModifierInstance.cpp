#include "ModifierInstance.h"

#include <sstream>
#include <magic_enum.hpp>

nejlika::ModifierInstance::ModifierInstance(const nlohmann::json& config) {
	type = magic_enum::enum_cast<ModifierType>(config["type"].get<std::string>()).value_or(ModifierType::Invalid);
	convertTo = magic_enum::enum_cast<ModifierType>(config["convert-to"].get<std::string>()).value_or(ModifierType::Invalid);
	value = config["value"].get<float>();
	
	if (config.contains("op")) {
		op = magic_enum::enum_cast<ModifierOperator>(config["op"].get<std::string>()).value_or(ModifierOperator::Additive);
	}
	else {
		op = ModifierOperator::Additive;
	}

	isResistance = config.contains("resistance") ? config["resistance"].get<bool>() : false;
	
	if (config.contains("category")) {
		category = magic_enum::enum_cast<ModifierCategory>(config["category"].get<std::string>()).value_or(ModifierCategory::Player);
	}
	else {
		category = ModifierCategory::Player;
	}

	effectID = config.contains("effect-id") ? config["effect-id"].get<uint32_t>() : 0;
	effectType = config.contains("effect-type") ? config["effect-type"].get<std::string>() : "";
}

nlohmann::json nejlika::ModifierInstance::ToJson() const
{
	nlohmann::json config;

	config["type"] = magic_enum::enum_name(type);
	config["convert-to"] = magic_enum::enum_name(convertTo);
	config["value"] = value;
	config["op"] = magic_enum::enum_name(op);
	config["resistance"] = isResistance;
	config["category"] = magic_enum::enum_name(category);
	config["effect-id"] = effectID;
	config["effect-type"] = effectType;

	return config;
}

std::string nejlika::ModifierInstance::GenerateHtmlString(const std::vector<ModifierInstance>& modifiers)
{
	std::stringstream ss;

	// target -> resistance -> op -> type -> value
	std::unordered_map<ModifierCategory, std::unordered_map<bool, std::unordered_map<ModifierOperator, std::unordered_map<ModifierType, float>>>> modifierMap;

	bool hasConvertTo = false;
	bool hasSkillModifier = false;
	
	for (const auto& modifier : modifiers) {
		if (modifier.type == ModifierType::Invalid) {
			continue;
		}

		if (modifier.GetConvertTo() != ModifierType::Invalid)
		{
			hasConvertTo = true;
			continue;
		}

		if (!modifier.GetUpgradeName().empty())
		{
			hasSkillModifier = true;
			continue;
		}

		modifierMap[modifier.category][modifier.isResistance][modifier.op][modifier.type] = modifier.value;
	}

	// Resistances and addatives are not separated, pet and player are
	// Summarize the resistances and addatives
	for (const auto& target : modifierMap) {
		if (target.first == ModifierCategory::Pet) {
			ss << "\n<font color=\"#D0AB62\">Pets:</font>\n";
		}
		
		for (const auto& resistance : target.second) {
			for (const auto& math : resistance.second) {
				for (const auto& modifier : math.second) {
					ss << "<font color=\"#FFFFFF\">";
					
					ss << ((modifier.second > 0) ? (math.first == ModifierOperator::Multiplicative ? "+" : "") : "-");
					
					ss << std::fixed << std::setprecision(1) << std::abs(modifier.second);

					if (math.first == ModifierOperator::Multiplicative) {
						ss << "%";
					}

					ss << "</font> <font color=\"#D0AB62\">";

					ss << " " << nejlika::GetModifierTypeName(modifier.first);	
					
					if (resistance.first) {
						// If the ss now ends with 'Damage' remove it
						if (ss.str().substr(ss.str().size() - 7) == " Damage") {
							ss.seekp(-7, std::ios_base::end);
						}

						ss << " " << "Resistance";
					}

					ss << "</font>\n";
				}
			}
		}
	}
	
	if (hasSkillModifier)
	{
		for (const auto& modifier : modifiers) {
			if (modifier.type != ModifierType::SkillModifier) {
				continue;
			}

			ss << "<font color=\"" << GetModifierTypeColor(modifier.type) << "\">";
			
			ss << ((modifier.value > 0) ? "+" : "-");
			
			ss << std::fixed << std::setprecision(0) << std::abs(modifier.value);

			ss << " to ";

			ss << modifier.GetUpgradeName();

			ss << "</font>\n";
		}
	}

	if (hasConvertTo)
	{
		for (const auto& modifier : modifiers) {
			if (modifier.GetConvertTo() == ModifierType::Invalid)
			{
				continue;
			}
			
			if (modifier.type == ModifierType::Invalid) {
				continue;
			}

			ss << "<font color=\"#FFFFFF\">";
			
			// +xx/yy% of T1 converted to T2
			ss << ((modifier.value > 0) ? "" : "-");

			ss << std::fixed << std::setprecision(0) << std::abs(modifier.value);

			ss << "%</font> <font color=\"#D0AB62\">";

			ss << " of ";

			ss << nejlika::GetModifierTypeName(modifier.type);

			ss << " converted to ";

			ss << nejlika::GetModifierTypeName(modifier.GetConvertTo());

			ss << "</font>\n";
		}
	}
	return ss.str();
}