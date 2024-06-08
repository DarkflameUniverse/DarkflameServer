#pragma once

#include "ModifierTemplate.h"
#include "UpgradeTriggerType.h"
#include "UpgradeTriggerCondition.h"
#include <InventoryComponent.h>

#include <dCommonVars.h>

namespace nejlika
{

class UpgradeEffect
{
public:
	UpgradeEffect(const nlohmann::json& json);

	nlohmann::json ToJson() const;
	
	std::vector<ModifierInstance> GenerateModifiers(int32_t level) const;

	void Load(const nlohmann::json& json);

	float CalculateChance(int32_t level) const;

	bool CheckConditions(LWOOBJID origin) const;

	void OnTrigger(LWOOBJID origin) const;

	static std::vector<ModifierInstance> Trigger(const std::vector<UpgradeEffect>& modifiers, int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin);

	// Getters

	const std::vector<ModifierTemplate>& GetModifiers() const { return modifiers; }

	UpgradeTriggerType GetTriggerType() const { return triggerType; }

	void AddSkill(LWOOBJID origin) const;
	void RemoveSkill(LWOOBJID origin) const;

private:
	struct UpgradeScale
	{
		int32_t level;
		float value;
	};
	
	std::vector<UpgradeScale> chance;
	std::vector<UpgradeTriggerCondition> conditions;
	UpgradeTriggerType triggerType;
	int32_t equipSkillID = 0;
	std::vector<ModifierTemplate> modifiers;
	int32_t effectID = 0;
	std::string effectType = "";
};

}
