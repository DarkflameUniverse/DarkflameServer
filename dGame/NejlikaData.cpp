#include "NejlikaData.h"

#include "Game.h"
#include "dConfig.h"
#include "json.hpp"
#include "Logger.h"

#include <fstream>

using namespace nejlika;

namespace
{

std::unordered_map<nejlika::ModifierNameType, std::vector<nejlika::ModifierNameTemplate>> modifierNameTemplates;

std::unordered_map<LWOOBJID, nejlika::AdditionalItemData> additionalItemData;

std::unordered_map<LWOOBJID, nejlika::AdditionalEntityData> additionalEntityData;

std::unordered_map<LOT, nejlika::EntityTemplate> entityTemplates;

std::unordered_map<LOT, nejlika::UpgradeTemplate> upgradeTemplates;

}

const std::unordered_map<ModifierNameType,std::vector<ModifierNameTemplate>>& nejlika::NejlikaData::GetModifierNameTemplates()
{
	return modifierNameTemplates;
}

const std::vector<nejlika::ModifierNameTemplate>& nejlika::NejlikaData::GetModifierNameTemplates(ModifierNameType type)
{
	const auto it = modifierNameTemplates.find(type);

	if (it != modifierNameTemplates.end()) {
		return it->second;
	}

	static const std::vector<nejlika::ModifierNameTemplate> empty;
	return empty;
}

const std::optional<AdditionalItemData*> nejlika::NejlikaData::GetAdditionalItemData(LWOOBJID id) {
	const auto& it = additionalItemData.find(id);

	if (it != additionalItemData.end()) {
		return std::optional<AdditionalItemData*>(&it->second);
	}
	
	return std::nullopt;
}

const std::optional<AdditionalEntityData*> nejlika::NejlikaData::GetAdditionalEntityData(LWOOBJID id) {
	const auto& it = additionalEntityData.find(id);

	if (it != additionalEntityData.end()) {
		return std::optional<AdditionalEntityData*>(&it->second);
	}

	return std::nullopt;
}

const std::optional<EntityTemplate*> nejlika::NejlikaData::GetEntityTemplate(LOT lot) {
	const auto& it = entityTemplates.find(lot);

	if (it != entityTemplates.end()) {
		return std::optional<EntityTemplate*>(&it->second);
	}

	return std::nullopt;
}

const std::optional<UpgradeTemplate*> nejlika::NejlikaData::GetUpgradeTemplate(LOT lot) {
	const auto& it = upgradeTemplates.find(lot);

	if (it != upgradeTemplates.end()) {
		return std::optional<UpgradeTemplate*>(&it->second);
	}

	return std::nullopt;
}

void nejlika::NejlikaData::SetAdditionalItemData(LWOOBJID id, AdditionalItemData data) {
	additionalItemData[id] = data;
}

void nejlika::NejlikaData::SetAdditionalEntityData(LWOOBJID id, AdditionalEntityData data) {
	additionalEntityData[id] = data;
}

void nejlika::NejlikaData::UnsetAdditionalItemData(LWOOBJID id) {
	additionalItemData.erase(id);
}

void nejlika::NejlikaData::UnsetAdditionalEntityData(LWOOBJID id) {
	additionalEntityData.erase(id);
}

void nejlika::NejlikaData::LoadNejlikaData()
{
	modifierNameTemplates.clear();

	// Load data from json file
	const auto& filename = Game::config->GetValue("nejlika");

	if (filename.empty())
	{
		return;
	}
	
	std::ifstream file(filename);

	if (!file.is_open())
	{
		LOG("Failed to open nejlika data file: %s", filename.c_str());
		return;
	}

	nlohmann::json json;
	
	try
	{
		json = nlohmann::json::parse(file);
	}
	catch (const nlohmann::json::exception& e)
	{
		LOG("Failed to parse nejlika data file: %s", e.what());
		return;
	}

	if (!json.contains("modifier-templates"))
	{
		LOG("nejlika data file does not contain modifier-templates");
		return;
	}

	const auto& modifierTemplates = json["modifier-templates"];

	for (const auto& value : modifierTemplates)
	{
		auto modifierTemplate = ModifierNameTemplate(value);

		modifierNameTemplates[modifierTemplate.GetType()].push_back(modifierTemplate);
	}

	LOG("Loaded %d modifier templates", modifierNameTemplates.size());

	if (json.contains("entity-templates"))
	{
		const auto& entityTemplatesArray = json["entity-templates"];
		
		for (const auto& value : entityTemplatesArray)
		{
			auto entityTemplate = EntityTemplate(value);

			entityTemplates[entityTemplate.GetLOT()] = entityTemplate;
		}
	}

	LOG("Loaded %d entity templates", entityTemplates.size());

	if (json.contains("upgrade-templates"))
	{
		const auto& upgradeTemplatesArray = json["upgrade-templates"];

		for (const auto& value : upgradeTemplatesArray)
		{
			auto upgradeTemplate = UpgradeTemplate(value);

			upgradeTemplates[upgradeTemplate.GetLot()] = upgradeTemplate;
		}
	}
}

