#pragma once

#include "ModifierTemplate.h"
#include "UpgradeTriggerType.h"

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

	static std::vector<ModifierInstance> Trigger(const std::vector<UpgradeEffect>& modifiers, int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin);

	// Getters

	const std::vector<ModifierTemplate>& GetModifiers() const { return modifiers; }

	UpgradeTriggerType GetTriggerType() const { return triggerType; }

private:
	struct UpgradeScale
	{
		int32_t level;
		float value;
	};
	
	std::vector<UpgradeScale> chance;
	UpgradeTriggerType triggerType;
	std::vector<ModifierTemplate> modifiers;
	int32_t effectID = 0;
	std::string effectType = "";
};

}
