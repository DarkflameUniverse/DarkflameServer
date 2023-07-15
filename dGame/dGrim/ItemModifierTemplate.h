#pragma once

#ifndef __ITEMMODIFIERTEMPLATE__H__
#define __ITEMMODIFIERTEMPLATE__H__

#include <cstdint>
#include <vector>

#include "StatProperty.h"
#include "StatRarity.h"
#include "eItemType.h"
#include "eLootSourceType.h"

struct ModifierRoll {
	std::vector<eItemType> itemTypes;
	int32_t minRatity;
	float chance;
	float levelMultiplier;
	float standardDeviation;
};

class ItemModifierTemplate {
public:
	ItemModifierTemplate(const std::string& name, eStatRarity rarity, bool isPrefix, bool isSuffix, const ModifierRoll& roll, int32_t priority = 0);
	~ItemModifierTemplate() = default;

	void AddStatProperty(const StatProperty& statProperty);

	const std::vector<StatProperty>& GetStatProperties() const;

	std::string HtmlString() const;

	const std::string& GetName() const;

	static void LoadItemModifierTemplates(const std::string& filename);

	static ItemModifierTemplate* FindItemModifierTemplate(const std::string& name);

	static void RollItemModifierTemplates(class Item* item, eLootSourceType lootSourceType);

	static std::string HtmlString(const std::vector<ItemModifierTemplate*>& itemModifierTemplates);

private:
	std::string m_Name;

	eStatRarity m_Rarity;

	ModifierRoll m_ModifierRoll;

	bool m_IsPrefix;
	bool m_IsSuffix;

	int32_t m_Priority;

	std::vector<StatProperty> m_StatProperties;

	static std::vector<ItemModifierTemplate> s_ItemModifierTemplates;
};

#endif  //!__ITEMMODIFIERTEMPLATE__H__