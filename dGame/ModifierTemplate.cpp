#include "ModifierTemplate.h"

#include <magic_enum.hpp>
#include <random>
#include <algorithm>

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

	if (!config.contains("scaling"))
	{
		throw std::runtime_error("Modifier template is missing scaling.");
	}
	
	const auto scaling = config["scaling"];

	for (const auto& scaler : scaling)
	{
		scales.push_back(ModifierScale(scaler));
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

	nlohmann::json scaling;

	for (const auto& scale : scales)
	{
		scaling.push_back(scale.ToJson());
	}

	config["scaling"] = scaling;

	config["category"] = magic_enum::enum_name(category);
	config["resistance"] = isResistance;
	config["effect-id"] = effectID;
	config["effect-type"] = effectType;

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

std::optional<ModifierInstance> nejlika::ModifierTemplate::GenerateModifier(ModifierType type, int32_t level) const {
	ModifierScale scale;
	bool found = false;

	// Select the scale with the highest level that is less than or equal to the current level
	for (const auto& s : scales) {
		if (s.GetLevel() <= level && s.GetLevel() > scale.GetLevel()) {
			scale = s;
			found = true;
		}
	}

	if (!found) {
		return std::nullopt;
	}

	float value = GeneralUtils::GenerateRandomNumber<float>(scale.GetMin(), scale.GetMax());

	return ModifierInstance(type, value, operatorType, isResistance, category, effectID, effectType);
}
