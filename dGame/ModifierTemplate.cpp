#include "ModifierTemplate.h"

#include <magic_enum.hpp>
#include <random>
#include <algorithm>
#include <sstream>
#include <iostream>

using namespace nejlika;

nejlika::ModifierTemplate::ModifierTemplate(const nlohmann::json& config) {
	if (config.contains("type"))
	{
		selector = ModifierTemplateSelector::One;

		const auto type = magic_enum::enum_cast<ModifierType>(config["type"].get<std::string>());

		if (type.has_value())
		{
			types = {type.value()};
		}
		else
		{
			types = {};
		}
	}
	else if (config.contains("all"))
	{
		selector = ModifierTemplateSelector::All;

		types = {};

		for (const auto& type : config["all"])
		{
			const auto t = magic_enum::enum_cast<ModifierType>(type.get<std::string>());

			if (t.has_value())
			{
				types.push_back(t.value());
			}
		}
	}
	else if (config.contains("two-of"))
	{
		selector = ModifierTemplateSelector::Two;

		types = {};

		for (const auto& type : config["two-of"])
		{
			const auto t = magic_enum::enum_cast<ModifierType>(type.get<std::string>());

			if (t.has_value())
			{
				types.push_back(t.value());
			}
		}
	}
	else
	{
		types = {};
	}

	if (config.contains("convert-to"))
	{
		convertTo = magic_enum::enum_cast<ModifierType>(config["convert-to"].get<std::string>()).value_or(ModifierType::Invalid);
	}
	else
	{
		convertTo = ModifierType::Invalid;
	}

	if (config.contains("scaling"))
	{
		const auto scaling = config["scaling"];

		for (const auto& scaler : scaling)
		{
			scales.push_back(ModifierScale(scaler));
		}
	}

	if (config.contains("polynomial"))
	{
		const auto polynomialConfig = config["polynomial"];

		for (const auto& term : polynomialConfig)
		{
			polynomial.push_back(term.get<float>());
		}
	}

	if (config.contains("category"))
	{
		category = magic_enum::enum_cast<ModifierCategory>(config["category"].get<std::string>()).value_or(ModifierCategory::Player);
	}
	else
	{
		category = ModifierCategory::Player;
	}

	isResistance = config.contains("resistance") ? config["resistance"].get<bool>() : false;

	effectID = config.contains("effect-id") ? config["effect-id"].get<uint32_t>() : 0;

	effectType = config.contains("effect-type") ? config["effect-type"].get<std::string>() : "";

	if (config.contains("operator"))
	{
		operatorType = magic_enum::enum_cast<ModifierOperator>(config["operator"].get<std::string>()).value_or(ModifierOperator::Additive);
	}
	else
	{
		operatorType = ModifierOperator::Additive;
	}

	// Old format
	if (config.contains("percentage"))
	{
		if (config["percentage"].get<bool>()) {
			operatorType = ModifierOperator::Multiplicative;
		}
	}

	if (config.contains("pet"))
	{
		if (config["pet"].get<bool>()) {
			category = ModifierCategory::Pet;
		}
	}

	if (config.contains("owner"))
	{
		if (config["owner"].get<bool>()) {
			category = ModifierCategory::Player;
		}
	}

	if (config.contains("skill"))
	{
		upgradeName = config["skill"].get<std::string>();
	}
}

nlohmann::json nejlika::ModifierTemplate::ToJson() const {
	nlohmann::json config;

	if (selector == ModifierTemplateSelector::One)
	{
		config["type"] = magic_enum::enum_name(types[0]);
	}
	else if (selector == ModifierTemplateSelector::All)
	{
		config["all"] = true;

		for (const auto& type : types)
		{
			config["types"].push_back(magic_enum::enum_name(type));
		}
	}
	else if (selector == ModifierTemplateSelector::Two)
	{
		config["two-of"] = true;

		for (const auto& type : types)
		{
			config["two-of"].push_back(magic_enum::enum_name(type));
		}
	}

	if (!scales.empty())
	{
		nlohmann::json scaling;

		for (const auto& scale : scales)
		{
			scaling.push_back(scale.ToJson());
		}

		config["scaling"] = scaling;
	}

	if (!polynomial.empty())
	{
		nlohmann::json polynomialConfig;

		for (const auto& term : polynomial)
		{
			polynomialConfig.push_back(term);
		}

		config["polynomial"] = polynomialConfig;
	}

	config["convert-to"] = magic_enum::enum_name(convertTo);

	config["category"] = magic_enum::enum_name(category);
	config["resistance"] = isResistance;
	config["effect-id"] = effectID;
	config["effect-type"] = effectType;
	config["operator"] = magic_enum::enum_name(operatorType);

	if (!upgradeName.empty())
	{
		config["skill"] = upgradeName;
	}

	return config;
}

std::vector<ModifierInstance> nejlika::ModifierTemplate::GenerateModifiers(int32_t level) const {
	std::vector<ModifierInstance> modifiers;

	std::vector<ModifierType> selectedTypes;

	if (types.empty())
	{
		return modifiers;
	}

	if (selector == ModifierTemplateSelector::One)
	{
		selectedTypes = {types[0]};
	}
	else if (selector == ModifierTemplateSelector::All)
	{
		selectedTypes = types;
	}
	else if (selector == ModifierTemplateSelector::Two)
	{
		if (types.size() < 2)
		{
			selectedTypes = types;
		}
		else
		{
			// Randomly select two types
			selectedTypes = types;

			std::shuffle(selectedTypes.begin(), selectedTypes.end(), std::mt19937(std::random_device()()));

			selectedTypes.resize(2);
		}
	}

	for (const auto& selectedType : selectedTypes)
	{
		auto modifierOpt = GenerateModifier(selectedType, level);

		if (modifierOpt.has_value())
		{
			modifiers.push_back(modifierOpt.value());
		}
	}

	return modifiers;
}

std::string nejlika::ModifierTemplate::GenerateHtmlString(const std::vector<ModifierTemplate>& modifiers, int32_t level) {
	std::stringstream ss;

	// target -> resistance -> op -> type -> (min, max)
	std::unordered_map<ModifierCategory, std::unordered_map<bool, std::unordered_map<ModifierOperator, std::unordered_map<ModifierType, std::pair<float, float>>>>> modifierMap;

	bool hasConvertTo = false;
	bool hasSkillModifier = false;

	for (const auto& modifier : modifiers) {
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

		for (const auto& type : modifier.types) {
			if (type == ModifierType::Invalid || type == ModifierType::SkillModifier) {
				continue;
			}

			if (!modifier.polynomial.empty())
			{
				float value = 0.0f;

				int32_t power = 0;
				for (const auto& term : modifier.polynomial)
				{
					value += term * std::pow(level, power);

					power++;
				}

				modifierMap[modifier.category][modifier.isResistance][modifier.operatorType][type] = {value, value};

				continue;
			}

			ModifierScale scale;
			bool found = false;

			// Select the scale with the highest level that is less than or equal to the current level
			for (const auto& s : modifier.scales) {
				if (s.GetLevel() <= level && s.GetLevel() > scale.GetLevel()) {
					scale = s;
					found = true;
				}
			}

			if (!found) {
				continue;
			}
			
			modifierMap[modifier.category][modifier.isResistance][modifier.operatorType][type] = {scale.GetMin(), scale.GetMax()};
		}
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

					ss << ((modifier.second.first > 0) ? (math.first == ModifierOperator::Multiplicative ? "+" : "") : "-");
					
					ss << std::fixed << std::setprecision(1) << std::abs(modifier.second.first);

					if (modifier.second.first != modifier.second.second)
					{
						ss << "/";

						ss << std::fixed << std::setprecision(1) << std::abs(modifier.second.second);
					}

					if (math.first == ModifierOperator::Multiplicative) {
						ss << "%";
					}
					
					ss << "</font> <font color=\"#D0AB62\">";

					ss << " " << nejlika::GetModifierTypeName(modifier.first);
					
					if (resistance.first) {
						// If the ss now ends with 'Damage' remove it
						if (ss.str().substr(ss.str().size() - 6) == "Damage") {
							ss.seekp(-6, std::ios_base::end);
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
			for (const auto& type : modifier.types) {
				if (type != ModifierType::SkillModifier) {
					continue;
				}

				const auto& scalors = modifier.GetScales();

				if (scalors.empty())
				{
					continue;
				}

				const auto& m = scalors[0];

				ss << "<font color=\"" << GetModifierTypeColor(type) << "\">";
				
				ss << ((m.GetMin() > 0) ? "+" : "-");
				
				ss << std::fixed << std::setprecision(0) << std::abs(m.GetMin());

				ss << " to ";

				ss << modifier.GetUpgradeName();

				ss << "</font>\n";
			}
		}
	}

	if (hasConvertTo)
	{
		for (const auto& modifier : modifiers) {
			if (modifier.GetConvertTo() == ModifierType::Invalid)
			{
				continue;
			}
			
			for (const auto& type : modifier.types) {
				if (type == ModifierType::Invalid) {
					continue;
				}

				const auto& scalors = modifier.GetScales();

				auto m = scalors[0];

				for (const auto& s : scalors) {
					if (s.GetLevel() <= level && s.GetLevel() > m.GetLevel()) {
						m = s;
					}
				}

				ss << "<font color=\"#FFFFFF\">";

				// +xx/yy% of T1 converted to T2
				ss << ((m.GetMin() > 0) ? "" : "-");

				ss << std::fixed << std::setprecision(0) << std::abs(m.GetMin());

				if (m.GetMin() != m.GetMax())
				{
					ss << "/";

					ss << std::fixed << std::setprecision(0) << std::abs(m.GetMax());
				}

				ss << "%</font> <font color=\"#D0AB62\">";

				ss << " of ";

				ss << nejlika::GetModifierTypeName(type);

				ss << " converted to ";

				ss << nejlika::GetModifierTypeName(modifier.GetConvertTo());

				ss << "</font>\n";
			}
		}
	}

	return ss.str();
}

std::optional<ModifierInstance> nejlika::ModifierTemplate::GenerateModifier(ModifierType type, int32_t level) const {
	ModifierScale scale;
	bool found = false;

	if (!polynomial.empty())
	{
		float value = 0.0f;

		int32_t power = 0;
		for (const auto& term : polynomial)
		{
			value += term * std::pow(level, power);

			power++;
		}

		return ModifierInstance(type, value, operatorType, isResistance, category, effectID, effectType, convertTo, upgradeName);
	}

	// Select the scale with the highest level that is less than or equal to the current level
	for (const auto& s : scales) {
		if ((s.GetLevel() <= level) && (s.GetLevel() > scale.GetLevel())) {
			std::cout << "Found scale: " << s.GetMin() << " - " << s.GetMax() << " for level " << s.GetLevel() << std::endl;
			scale = s;
			found = true;
		}
	}

	if (!found) {
		return std::nullopt;
	}

	float value = 0;
	
	if (scale.GetMax() == scale.GetMin())
	{
		value = scale.GetMin();
	}
	else
	{
		value = (GeneralUtils::GenerateRandomNumber<uint32_t>(0, 100) / 100.0f) * (scale.GetMax() - scale.GetMin()) + scale.GetMin();
	}
	
	std::cout << "Generated modifier: " << value << " with level " << level << " for type: " << magic_enum::enum_name(type) << std::endl;

	return ModifierInstance(type, value, operatorType, isResistance, category, effectID, effectType, convertTo, upgradeName);
}
