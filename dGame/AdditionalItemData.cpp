#include "AdditionalItemData.h"

#include "Item.h"
#include "eItemType.h"
#include "NejlikaData.h"

using namespace nejlika;

nejlika::AdditionalItemData::AdditionalItemData(Item* item)
{
	const auto& config = item->GetConfig();

	for (const auto& entry : config)
	{
		if (entry->GetKey() != u"modifiers")
		{
			continue;
		}

		const auto str = entry->GetValueAsString();

		if (str.empty())
		{
			continue;
		}

		try
		{
			const auto json = nlohmann::json::parse(str);

			Load(json);
		}
		catch (const nlohmann::json::exception& e)
		{
			std::cout << "Failed to parse additional item data: " << e.what() << std::endl;
		}
	}
}

nejlika::AdditionalItemData::AdditionalItemData(const nlohmann::json& json) {
	Load(json);
}

void nejlika::AdditionalItemData::Load(const nlohmann::json& json) {
	if (json.contains("names"))
	{
		for (const auto& name : json["names"])
		{
			modifierNames.emplace_back(name);
		}
	}

	if (json.contains("instances"))
	{
		for (const auto& instance : json["instances"])
		{
			modifierInstances.emplace_back(instance);
		}
	}
}

nlohmann::json nejlika::AdditionalItemData::ToJson() const {
	nlohmann::json json;

	json["names"] = nlohmann::json::array();

	for (const auto& name : modifierNames)
	{
		json["names"].push_back(name.ToJson());
	}

	json["instances"] = nlohmann::json::array();

	for (const auto& instance : modifierInstances)
	{
		json["instances"].push_back(instance.ToJson());
	}

	return json;
}

void nejlika::AdditionalItemData::Save(Item* item) {
	auto& config = item->GetConfig();
	
	// Remove the old data
	for (size_t i = 0; i < config.size(); i++)
	{
		if (config[i]->GetKey() == u"modifiers")
		{
			config.erase(config.begin() + i);
			break;
		}
	}
	
	std::stringstream ss;

	ss << ToJson().dump();

	std::cout << ss.str() << std::endl;

	config.push_back(new LDFData<std::string>(u"modifiers", ToJson().dump()));
}

void nejlika::AdditionalItemData::RollModifiers(Item* item, int32_t level) {
	modifierNames.clear();
	modifierInstances.clear();

	const auto& info = item->GetInfo();

	const auto itemType = static_cast<eItemType>(info.itemType);
	const auto itemRarity = info.rarity == 0 ? 1 : info.rarity;

	uint32_t rarityRollPrefix = 0;
	uint32_t rarityRollSuffix = 0;

	// Generate (itemRarity) amout of names and modifiers rolls, take the highest rarity. 0-1000
	for (int i = 0; i < itemRarity; i++) {
		auto roll = GeneralUtils::GenerateRandomNumber<uint32_t>() % 1000;

		if (roll > rarityRollPrefix) {
			rarityRollPrefix = roll;
		}
		
		roll = GeneralUtils::GenerateRandomNumber<uint32_t>() % 1000;

		if (roll > rarityRollSuffix) {
			rarityRollSuffix = roll;
		}
	}
	
	const auto& templates = NejlikaData::GetModifierNameTemplates();

	std::vector<ModifierNameTemplate> availablePrefixes;
	std::vector<ModifierNameTemplate> availableSuffixes;

	for (const auto& [type, nameTemplates] : templates) {
		for (const auto& nameTemplate : nameTemplates) {
			if (type != ModifierNameType::Prefix && type != ModifierNameType::Suffix) {
				continue;
			}

			if (nameTemplate.GetMinLevel() > level || nameTemplate.GetMaxLevel() < level) {
				continue;
			}

			const auto rarity = nameTemplate.GetRarity();

			if (rarity == ModifierRarity::Common) {
				continue;
			}

			const auto& itemTypes = nameTemplate.GetItemTypes();

			if (std::find(itemTypes.begin(), itemTypes.end(), itemType) == itemTypes.end()) {
				continue;
			}
			
			/*
				Uncommon: rarityRoll > 500,
				Rare: rarityRoll > 900,
				Epic: rarityRoll > 990,
				Legendary: rarityRoll = 999
			*/
			const auto roll = type == ModifierNameType::Prefix ? rarityRollPrefix : rarityRollSuffix;

			if (rarity == ModifierRarity::Uncommon && roll > 900) {
				continue;
			}
			
			if (rarity == ModifierRarity::Rare && (roll <= 900 || roll > 990)) {
				continue;
			}

			if (rarity == ModifierRarity::Epic && (roll <= 990 || roll > 998)) {
				continue;
			}

			if (rarity == ModifierRarity::Legendary && roll != 999) {
				continue;
			}

			if (type == ModifierNameType::Prefix) {
				availablePrefixes.push_back(nameTemplate);
			}
			else {
				availableSuffixes.push_back(nameTemplate);
			}
		}
	}

	if (!availablePrefixes.empty()) {
		const auto& prefix = availablePrefixes[GeneralUtils::GenerateRandomNumber<uint32_t>() % availablePrefixes.size()];

		modifierNames.push_back(ModifierName(prefix));
		
		const auto modifiers = prefix.GenerateModifiers(level);

		modifierInstances.insert(modifierInstances.end(), modifiers.begin(), modifiers.end());
	}

	if (!availableSuffixes.empty()) {
		const auto& suffix = availableSuffixes[GeneralUtils::GenerateRandomNumber<uint32_t>() % availableSuffixes.size()];

		modifierNames.push_back(ModifierName(suffix));

		const auto modifiers = suffix.GenerateModifiers(level);

		modifierInstances.insert(modifierInstances.end(), modifiers.begin(), modifiers.end());
	}

	const auto& itemTemplateVec = NejlikaData::GetModifierNameTemplates(ModifierNameType::Object);

	std::vector<const ModifierNameTemplate*> availableObjects;

	for (const auto& itemTemplate : itemTemplateVec) {
		if (itemTemplate.GetMinLevel() > level || itemTemplate.GetMaxLevel() < level) {
			continue;
		}

		if (itemTemplate.GetLOT() != static_cast<int32_t>(item->GetLot())) {
			continue;
		}

		availableObjects.push_back(&itemTemplate);
	}

	if (availableObjects.empty()) {
		Save(item);
		return;
	}

	const auto& itemTemplate = *availableObjects[GeneralUtils::GenerateRandomNumber<uint32_t>() % availableObjects.size()];

	const auto itemModifiers = itemTemplate.GenerateModifiers(level);

	modifierInstances.insert(modifierInstances.end(), itemModifiers.begin(), itemModifiers.end());

	Save(item);
}

