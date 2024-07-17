#include "EntityTemplate.h"

#include <magic_enum.hpp>

nejlika::EntityTemplate::EntityTemplate(const nlohmann::json& json) {
	lot = json["lot"].get<LOT>();
	minLevel = json.contains("min-level") ? json["min-level"].get<int32_t>() : 1;

	for (const auto& scaler : json["scaling"])
	{
		EntityTemplateScaler s;

		s.type = magic_enum::enum_cast<ModifierType>(scaler["type"].get<std::string>()).value();
		s.isResistance = scaler.contains("resistance") && scaler["resistance"].get<bool>();
		s.polynomial = scaler["polynomial"].get<std::vector<float>>();

		scalers.push_back(s);
	}
}

nlohmann::json nejlika::EntityTemplate::ToJson() const {
	nlohmann::json json;

	json["lot"] = lot;
	json["min-level"] = minLevel;

	nlohmann::json scalersJson;

	for (const auto& scaler : scalers)
	{
		nlohmann::json s;

		s["type"] = magic_enum::enum_name(scaler.type);
		s["resistance"] = scaler.isResistance;
		s["polynomial"] = scaler.polynomial;

		scalersJson.push_back(s);
	}

	json["scaling"] = scalersJson;

	return json;
}

float nejlika::EntityTemplate::GetScaler(ModifierType type, bool isResistance, int32_t level) const {
	for (const auto& scaler : scalers)
	{
		if (scaler.type == type && scaler.isResistance == isResistance)
		{
			return CalculateScaler(scaler, level);
		}
	}

	return 0.0f;
}

std::vector<nejlika::ModifierInstance> nejlika::EntityTemplate::GenerateModifiers(int32_t level) const {
	std::vector<ModifierInstance> modifiers;

	for (const auto& scaler : scalers)
	{
		ModifierInstance modifier(
			scaler.type,
			CalculateScaler(scaler, level),
			ModifierOperator::Additive,
			scaler.isResistance,
			ModifierCategory::Player,
			0,
			"",
			ModifierType::Invalid,
			""
		);

		modifiers.push_back(modifier);
	}

	return modifiers;
}

float nejlika::EntityTemplate::CalculateScaler(const EntityTemplateScaler& scaler, int32_t level) const {
	float result = 0.0f;

	for (size_t i = 0; i < scaler.polynomial.size(); ++i)
	{
		result += scaler.polynomial[i] * std::pow(level, i);
	}

	return result;
}

