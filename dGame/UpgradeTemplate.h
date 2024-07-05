#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"

#include "UpgradeEffect.h"
#include "TriggerParameters.h"

namespace nejlika
{

class UpgradeTemplate
{
public:
	UpgradeTemplate() = default;

	UpgradeTemplate(const nlohmann::json& json);

	nlohmann::json ToJson() const;

	void Load(const nlohmann::json& json);

	const std::string& GetName() const { return name; }
	int32_t GetLot() const { return lot; }
	int32_t GetMaxLevel() const { return maxLevel; }
	const std::vector<UpgradeEffect>& GetPassives() const { return passives; }
	const std::vector<ModifierTemplate>& GetModifiers() const { return modifiers; }

	std::vector<ModifierInstance> Trigger(int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin, const TriggerParameters& params) const;

	std::vector<ModifierInstance> GenerateModifiers(int32_t level) const;

	void AddSkills(LWOOBJID origin) const;
	void RemoveSkills(LWOOBJID origin) const;

private:
	std::string name = "";
	int32_t lot = 0;
	int32_t maxLevel = 0;
    std::vector<UpgradeEffect> passives;
	std::vector<ModifierTemplate> modifiers;

};

}