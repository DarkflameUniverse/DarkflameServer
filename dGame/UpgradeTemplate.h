#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "json.hpp"

#include "UpgradeEffect.h"

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

	std::vector<ModifierInstance> Trigger(int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin) const;

private:
	std::string name = "";
	int32_t lot = 0;
	int32_t maxLevel = 0;
    std::vector<UpgradeEffect> passives;

};

}