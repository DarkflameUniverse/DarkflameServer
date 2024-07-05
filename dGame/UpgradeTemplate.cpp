#include "UpgradeTemplate.h"

#include "NejlikaData.h"

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
	if (json["lot"].is_string()) {
		lot = NejlikaData::GetLookup().GetValue(json["lot"].get<std::string>());
	}
	else {
		lot = json["lot"].get<int32_t>();
	}
	maxLevel = json.contains("max-level") ? json["max-level"].get<int32_t>() : 1;

	passives.clear();

	if (json.contains("modifiers")) {
		for (const auto& modifier : json["modifiers"]) {
			ModifierTemplate modTemplate(modifier);
			modifiers.push_back(modTemplate);
		}
	}

	if (json.contains("passives")) {
		for (const auto& passive : json["passives"]) {
			UpgradeEffect effect(passive);
			passives.push_back(effect);
		}
	}
}

std::vector<ModifierInstance> nejlika::UpgradeTemplate::Trigger(int32_t level, UpgradeTriggerType triggerType, LWOOBJID origin, const TriggerParameters& params) const {
	level = std::min(level, maxLevel);

	return UpgradeEffect::Trigger(passives, level, triggerType, origin, params);
}

std::vector<ModifierInstance> nejlika::UpgradeTemplate::GenerateModifiers(int32_t level) const {
	level = std::min(level, maxLevel);

	std::vector<ModifierInstance> result;

	for (const auto& modifier : modifiers) {
		auto instances = modifier.GenerateModifiers(level);

		result.insert(result.end(), instances.begin(), instances.end());
	}

	return result;
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

