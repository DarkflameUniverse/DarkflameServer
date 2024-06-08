#include "UpgradeEffect.h"

#include "GeneralUtils.h"
#include "GameMessages.h"
#include "InventoryComponent.h"

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

	if (!conditions.empty()) {
		nlohmann::json conditionsJson = nlohmann::json::array();

		for (const auto& condition : conditions) {
			conditionsJson.push_back(magic_enum::enum_name(condition));
		}

		json["conditions"] = conditionsJson;
	}

	if (equipSkillID != 0) {
		json["grant-skill-id"] = equipSkillID;
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

	if (json.contains("modifiers")){
		for (const auto& modifier : json["modifiers"]) {
			ModifierTemplate effect(modifier);
			modifiers.push_back(effect);
		}
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

	if (json.contains("conditions")) {
		conditions.clear();

		for (const auto& condition : json["conditions"]) {
			conditions.push_back(magic_enum::enum_cast<UpgradeTriggerCondition>(condition.get<std::string>()).value_or(UpgradeTriggerCondition::None));
		}
	}

	if (json.contains("grant-skill-id")) {
		equipSkillID = json["grant-skill-id"].get<int32_t>();
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

bool nejlika::UpgradeEffect::CheckConditions(LWOOBJID origin) const {
	auto* entity = Game::entityManager->GetEntity(origin);

	if (!entity) {
		return false;
	}

	auto* inventory = entity->GetComponent<InventoryComponent>();

	if (!inventory) {
		return false;
	}

	const auto& skills = inventory->GetSkills();
	const auto& equipped = inventory->GetEquippedItems();

	for (const auto& condition : conditions) {
		switch (condition) {
		case UpgradeTriggerCondition::None:
			break;
		case UpgradeTriggerCondition::Unarmed:
			if (equipped.contains("special_r")) {
				return false;
			}
			break;
		case UpgradeTriggerCondition::Melee:
			if (!equipped.contains("special_r")) {
				return false;
			}
			break;
		case UpgradeTriggerCondition::TwoHanded:
		{
			if (!equipped.contains("special_r")) {
				return false;
			}

			const auto& weaponLot = equipped.at("special_r").lot;

			const auto& info = Inventory::FindItemComponent(weaponLot);

			if (!info.isTwoHanded) {
				return false;
			}
			break;
		}
		case UpgradeTriggerCondition::Shield:
			if (!equipped.contains("special_l")) {
				return false;
			}
			break;
		default:
			break;
		}
	}

	return true;
}

void nejlika::UpgradeEffect::OnTrigger(LWOOBJID origin) const {
	auto* entity = Game::entityManager->GetEntity(origin);

	if (!entity) {
		return;
	}

	auto* inventory = entity->GetComponent<InventoryComponent>();

	if (!inventory) {
		return;
	}
}

std::vector<ModifierInstance> nejlika::UpgradeEffect::Trigger(const std::vector<UpgradeEffect>& modifiers, int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin) {
	std::vector<ModifierInstance> result;

	for (const auto& modifier : modifiers) {
		if (modifier.GetTriggerType() != triggerType) {
			continue;
		}

		if (!modifier.CheckConditions(origin)) {
			continue;
		}

		float chanceRoll = GeneralUtils::GenerateRandomNumber<float>(0, 1);

		if (chanceRoll > modifier.CalculateChance(level)) {
			continue;
		}

		std::cout << "Triggering effect trigger type: " << magic_enum::enum_name(triggerType) << std::endl;

		modifier.OnTrigger(origin);

		auto instances = modifier.GenerateModifiers(level);

		result.insert(result.end(), instances.begin(), instances.end());

		if (modifier.effectID == 0) {
			continue;
		}

		GameMessages::SendPlayFXEffect(
			origin,
			modifier.effectID,
			GeneralUtils::UTF8ToUTF16(modifier.effectType),
			std::to_string(GeneralUtils::GenerateRandomNumber<size_t>())
		);
	}

	return result;
}

void nejlika::UpgradeEffect::AddSkill(LWOOBJID origin) const {
	auto* entity = Game::entityManager->GetEntity(origin);

	if (!entity) {
		return;
	}

	auto* inventory = entity->GetComponent<InventoryComponent>();

	if (!inventory) {
		return;
	}

	if (triggerType != UpgradeTriggerType::Active) {
		return;
	}

	if (equipSkillID != 0) {
		inventory->SetSkill(equipSkillID);
	}
}

void nejlika::UpgradeEffect::RemoveSkill(LWOOBJID origin) const {
	auto* entity = Game::entityManager->GetEntity(origin);

	if (!entity) {
		return;
	}

	auto* inventory = entity->GetComponent<InventoryComponent>();

	if (!inventory) {
		return;
	}

	if (triggerType != UpgradeTriggerType::Active) {
		return;
	}

	if (equipSkillID != 0) {
		inventory->UnsetSkill(equipSkillID);
	}
}
