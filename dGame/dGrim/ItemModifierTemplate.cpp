#include "ItemModifierTemplate.h"

#include <sstream>
#include <tinyxml2.h>
#include "Item.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "LevelProgressionComponent.h"

std::vector<ItemModifierTemplate> ItemModifierTemplate::s_ItemModifierTemplates;

void ItemModifierTemplate::LoadItemModifierTemplates(const std::string& filename) {
	std::vector<ItemModifierTemplate> itemModifierTemplates;

	tinyxml2::XMLDocument doc;
	doc.LoadFile(filename.c_str());

	tinyxml2::XMLElement* root = doc.FirstChildElement("Templates");

	for (tinyxml2::XMLElement* element = root->FirstChildElement("Template"); element != nullptr; element = element->NextSiblingElement("Template")) {
		std::string name = element->Attribute("name");
		eStatRarity rarity = static_cast<eStatRarity>(element->IntAttribute("rarity"));
		bool isPrefix = element->BoolAttribute("isPrefix");
		bool isSuffix = element->BoolAttribute("isSuffix");
		int32_t priority = element->IntAttribute("priority");

		ModifierRoll roll;
		roll.chance = element->FloatAttribute("chance");
		roll.levelMultiplier = element->FloatAttribute("levelMultiplier");
		roll.minRatity = element->IntAttribute("minRarity");
		roll.standardDeviation = element->FloatAttribute("standardDeviation");

		std::string itemTypes = element->Attribute("itemTypes");
		std::stringstream ss(itemTypes);
		std::string itemType;
		while (std::getline(ss, itemType, ',')) {
			roll.itemTypes.push_back(static_cast<eItemType>(std::stoi(itemType)));
		}

		ItemModifierTemplate itemModifierTemplate(name, rarity, isPrefix, isSuffix, roll, priority);

		for (tinyxml2::XMLElement* statElement = element->FirstChildElement("Stat"); statElement != nullptr; statElement = statElement->NextSiblingElement("Stat")) {
			eStatTypes type = static_cast<eStatTypes>(statElement->IntAttribute("type"));
			eStatModifier modifier = static_cast<eStatModifier>(statElement->IntAttribute("modifier"));
			float value = statElement->FloatAttribute("value");

			StatProperty statProperty(type, modifier, value);

			itemModifierTemplate.AddStatProperty(statProperty);
		}

		itemModifierTemplates.push_back(itemModifierTemplate);
	}

	s_ItemModifierTemplates = itemModifierTemplates;
}

ItemModifierTemplate* ItemModifierTemplate::FindItemModifierTemplate(const std::string& name) {
	for (ItemModifierTemplate& itemModifierTemplate : s_ItemModifierTemplates) {
		if (itemModifierTemplate.m_Name == name) {
			return &itemModifierTemplate;
		}
	}

	return nullptr;
}

void ItemModifierTemplate::RollItemModifierTemplates(Item* item, eLootSourceType lootSourceType) {
	if (item->GetLot() == 6086) return;

	auto* entity = item->GetInventory()->GetComponent()->GetParent();

	auto* levelProgressionComponent = entity->GetComponent<LevelProgressionComponent>();

	if (levelProgressionComponent == nullptr) {
		return;
	}

	int32_t level = levelProgressionComponent->GetLevel();

	std::vector<ItemModifierTemplate*> prefixes;
	std::vector<ItemModifierTemplate*> suffixes;
	std::vector<StatProperty> statProperties;

	const auto& itemInfo = item->GetInfo();

	int32_t rarity = itemInfo.rarity;
	int32_t value = itemInfo.baseValue;

	for (ItemModifierTemplate& itemModifierTemplate : s_ItemModifierTemplates) {
		const auto& rollInfo = itemModifierTemplate.m_ModifierRoll;

		if (rollInfo.minRatity > rarity) {
			continue;
		}

		if (rollInfo.itemTypes.size() > 0) {
			bool found = false;
			for (eItemType itemType : rollInfo.itemTypes) {
				if (itemType == static_cast<eItemType>(itemInfo.itemType)) {
					found = true;
					break;
				}
			}

			if (!found) {
				continue;
			}
		}

		float chance = rollInfo.chance;

		float rng = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

		if (rng > chance) {
			continue;
		}

		if (itemModifierTemplate.m_IsPrefix) {
			prefixes.push_back(&itemModifierTemplate);
		}
		else if (itemModifierTemplate.m_IsSuffix) {
			suffixes.push_back(&itemModifierTemplate);
		}
	}

	// Randomize order of prefixes and suffixes
	std::random_shuffle(prefixes.begin(), prefixes.end());
	std::random_shuffle(suffixes.begin(), suffixes.end());

	// Add the first prefix and suffix
	if (prefixes.size() > 0) {
		ItemModifierTemplate* prefix = prefixes[0];

		item->GetModifiers().push_back(prefix);
	}

	if (suffixes.size() > 0) {
		ItemModifierTemplate* suffix = suffixes[0];

		item->GetModifiers().push_back(suffix);
	}

	// If there are more than one prefix or suffix, there is a 0.05 chance to add another one
	if (prefixes.size() > 1) {
		float rng = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

		if (rng < 0.05f) {
			ItemModifierTemplate* prefix = prefixes[1];

			item->GetModifiers().push_back(prefix);
		}
	}

	if (suffixes.size() > 1) {
		float rng = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

		if (rng < 0.05f) {
			ItemModifierTemplate* suffix = suffixes[1];

			item->GetModifiers().push_back(suffix);
		}
	}

	// Add stat properties from prefixes and suffixes
	for (ItemModifierTemplate* itemModifierTemplate : item->GetModifiers()) {
		const std::vector<StatProperty>& templateStatProperties = itemModifierTemplate->GetStatProperties();

		auto rollInfo = itemModifierTemplate->m_ModifierRoll;

		// Roll stat properties
		for (const StatProperty& statProperty : templateStatProperties) {
			float value = statProperty.value;

			float rng = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
			
			float standardDeviation = rollInfo.standardDeviation;

			// (+/-) standardDeviation
			float deviation = value * ((rng * standardDeviation * 2.0f) - standardDeviation);

			value += deviation;

			float levelMultiplier = rollInfo.levelMultiplier;

			float multiplier = 1.0f + (level * levelMultiplier);

			switch (itemInfo.rarity)
			{
			case 0:
				break;
			case 1:
				multiplier += 0.0f;
				break;
			case 2:
				multiplier += 0.5f;
				break;
			case 3:
				multiplier += 0.75f;
				break;
			case 4:
				multiplier += 1.25f;
				break;

			
			default:
				break;
			}

			value *= multiplier;

			if (itemInfo.isTwoHanded) {
				value *= 1.75f;
			}

			if (lootSourceType == eLootSourceType::VENDOR) {
				value *= 0.75f;
			}

			// Round to 2 decimal places
			value = static_cast<float>(static_cast<int32_t>(value * 100.0f)) / 100.0f;

			StatProperty newStatProperty(statProperty.type, statProperty.modifier, value);

			statProperties.push_back(newStatProperty);
		}
	}

	// Add stat properties
	for (const StatProperty& statProperty : statProperties) {
		item->GetStats().push_back(statProperty);
		
	}
}

ItemModifierTemplate::ItemModifierTemplate(const std::string& name, eStatRarity rarity, bool isPrefix, bool isSuffix, const ModifierRoll& roll, int32_t priority) {
	this->m_Name = name;
	this->m_Rarity = rarity;
	this->m_IsPrefix = isPrefix;
	this->m_IsSuffix = isSuffix;
	this->m_Priority = priority;
	this->m_ModifierRoll = roll;
	this->m_StatProperties = {};
}

void ItemModifierTemplate::AddStatProperty(const StatProperty& statProperty) {
	m_StatProperties.push_back(statProperty);
}

const std::vector<StatProperty>& ItemModifierTemplate::GetStatProperties() const {
	return m_StatProperties;
}

std::string ItemModifierTemplate::HtmlString() const {
	std::stringstream ss;
	ss << "<font color=\"#";

	switch (m_Rarity)
	{
	case eStatRarity::Common:
		ss << "FFFFFF";
		break;
	case eStatRarity::Uncommon:
		ss << "00FF00";
		break;
	case eStatRarity::Rare:
		ss << "0077FF";
		break;
	case eStatRarity::Epic:
		ss << "FF00FF";
		break;
	case eStatRarity::Legendary:
		ss << "FF7700";
		break;
	case eStatRarity::Relic:
		ss << "FFC391";
		break;
	default:
		ss << "FFFFFF";
		break;
	}

	ss << "\">";

	ss << m_Name << "</font>\n";

	return ss.str();
}

std::string ItemModifierTemplate::HtmlString(const std::vector<ItemModifierTemplate*>& itemModifierTemplates) {
	/*
		Prefix-1 Prefix-2 NAME Suffix-1 Suffix-2
	*/
	std::stringstream ss;

	for (ItemModifierTemplate* itemModifierTemplate : itemModifierTemplates) {
		if (itemModifierTemplate->m_IsPrefix) {
			ss << itemModifierTemplate->HtmlString();
		}
	}

	ss << "<font color=\"#56B555\">NAME</font>";

	for (ItemModifierTemplate* itemModifierTemplate : itemModifierTemplates) {
		if (itemModifierTemplate->m_IsSuffix) {
			ss << itemModifierTemplate->HtmlString();
		}
	}

	return ss.str();
}

const std::string& ItemModifierTemplate::GetName() const {
	return m_Name;
}
