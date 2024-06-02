#include "UpgradeEffect.h"

#include "GeneralUtils.h"
#include "GameMessages.h"

#include <magic_enum.hpp>
#include <iostream>

using namespace nejlika;

nejlika::UpgradeEffect::UpgradeEffect(const nlohmann::json& json)
{
	Load(json);
}

nlohmann::json nejlika::UpgradeEffect::ToJson() const
{
	nlohmann::json json;

	json["trigger-type"] = static_cast<int32_t>(triggerType);

	nlohmann::json modifiersJson = nlohmann::json::array();

	for (const auto& modifier : modifiers) {
		modifiersJson.push_back(modifier.ToJson());
	}

	json["modifiers"] = modifiersJson;
	
	if (!chance.empty()) {
		nlohmann::json chanceJson = nlohmann::json::array();

		for (const auto& scale : chance) {
			chanceJson.push_back({
				{"level", scale.level},
				{"value", scale.value}
			});
		}

		json["chance"] = chanceJson;
	}

	if (effectID != 0) {
		json["effect-id"] = effectID;
	}

	if (!effectType.empty()) {
		json["effect-type"] = effectType;
	}

	return json;
}

std::vector<ModifierInstance> nejlika::UpgradeEffect::GenerateModifiers(int32_t level) const
{
	std::vector<ModifierInstance> result;

	for (const auto& modifier : modifiers) {
		auto instances = modifier.GenerateModifiers(level);

		result.insert(result.end(), instances.begin(), instances.end());
	}

	return result;
}

void nejlika::UpgradeEffect::Load(const nlohmann::json& json)
{
	triggerType = magic_enum::enum_cast<UpgradeTriggerType>(json["trigger-type"].get<std::string>()).value_or(UpgradeTriggerType::OnHit);

	modifiers.clear();

	for (const auto& modifier : json["modifiers"]) {
		ModifierTemplate effect(modifier);
		modifiers.push_back(effect);
	}

	if (json.contains("chance")) {
		chance.clear();

		for (const auto& scale : json["chance"]) {
			chance.push_back({
				scale["level"].get<int32_t>(),
				scale["value"].get<float>()
			});
		}
	}

	if (json.contains("effect-id")) {
		effectID = json["effect-id"].get<int32_t>();
	}

	if (json.contains("effect-type")) {
		effectType = json["effect-type"].get<std::string>();
	}
}

float nejlika::UpgradeEffect::CalculateChance(int32_t level) const {
	if (chance.empty()) {
		return 1;
	}

	// Find the highest level that is less than or equal to the given level
	float value = 0;

	for (const auto& scale : chance) {
		if (scale.level <= level) {
			value = scale.value;
		}
	}

	return value;
}

std::vector<ModifierInstance> nejlika::UpgradeEffect::Trigger(const std::vector<UpgradeEffect>& modifiers, int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin) {
	std::vector<ModifierInstance> result;

	for (const auto& modifier : modifiers) {
		if (modifier.GetTriggerType() != triggerType) {
			continue;
		}

		float chanceRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

		if (chanceRoll > modifier.CalculateChance(level)) {
			continue;
		}

		auto instances = modifier.GenerateModifiers(level);

		result.insert(result.end(), instances.begin(), instances.end());

		GameMessages::SendPlayFXEffect(
			origin,
			modifier.effectID,
			GeneralUtils::UTF8ToUTF16(modifier.effectType),
			std::to_string(GeneralUtils::GenerateRandomNumber<size_t>())
		);
	}

	return result;
}
