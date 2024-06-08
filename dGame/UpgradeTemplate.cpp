#include "UpgradeTemplate.h"

using namespace nejlika;

nejlika::UpgradeTemplate::UpgradeTemplate(const nlohmann::json& json)
{
	Load(json);
}

nlohmann::json nejlika::UpgradeTemplate::ToJson() const
{
	nlohmann::json json;

	json["name"] = name;
	json["lot"] = lot;
	json["max-level"] = maxLevel;

	nlohmann::json passivesJson = nlohmann::json::array();

	for (const auto& passive : passives) {
		passivesJson.push_back(passive.ToJson());
	}

	json["passives"] = passivesJson;

	return json;
}

void nejlika::UpgradeTemplate::Load(const nlohmann::json& json)
{
	name = json["name"].get<std::string>();
	lot = json["lot"].get<int32_t>();
	maxLevel = json["max-level"].contains("max-level") ? json["max-level"].get<int32_t>() : 1;

	passives.clear();

	for (const auto& passive : json["passives"]) {
		UpgradeEffect effect(passive);
		passives.push_back(effect);
	}
}

std::vector<ModifierInstance> nejlika::UpgradeTemplate::Trigger(int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin) const {
	level = std::min(level, maxLevel);

	return UpgradeEffect::Trigger(passives, level, triggerType, origin);
}

void nejlika::UpgradeTemplate::AddSkills(LWOOBJID origin) const {
	for (const auto& passive : passives) {
		passive.AddSkill(origin);
	}
}

void nejlika::UpgradeTemplate::RemoveSkills(LWOOBJID origin) const {
	for (const auto& passive : passives) {
		passive.RemoveSkill(origin);
	}
}

