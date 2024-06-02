#include "ModifierNameTemplate.h"

#include <iostream>

#include "magic_enum.hpp"

using namespace nejlika;

nejlika::ModifierNameTemplate::ModifierNameTemplate(const nlohmann::json & json)
{
	name = json["name"].get<std::string>();

	if (json.contains("lot"))
	{
		lot = json["lot"].get<int32_t>();
	}
	else
	{
		lot = 0;
	}
	
	if (json.contains("type"))
	{
		type = magic_enum::enum_cast<ModifierNameType>(json["type"].get<std::string>()).value_or(ModifierNameType::Prefix);
	}
	else
	{
		type = ModifierNameType::Prefix;
	}

	if (json.contains("items"))
	{
		for (const auto& itemType : json["items"])
		{
			std::string type = itemType.get<std::string>();

			// Make uppercase
			std::transform(type.begin(), type.end(), type.begin(), ::toupper);

			// Replace spaces with underscores
			std::replace(type.begin(), type.end(), ' ', '_');

			const auto itemTypeEnum = magic_enum::enum_cast<eItemType>(type);

			if (itemTypeEnum.has_value())
			{
				itemTypes.push_back(itemTypeEnum.value());
			}
			else
			{
				std::cout << "Invalid item type: " << type << std::endl;
			}
		}
	}

	if (json.contains("modifiers"))
	{
		for (const auto& modifier : json["modifiers"])
		{
			modifiers.push_back(ModifierTemplate(modifier));
		}
	}

	if (json.contains("levels"))
	{
		auto levels = json["levels"];

		if (levels.contains("min"))
		{
			minLevel = levels["min"].get<int32_t>();
		}
		else
		{
			minLevel = 1;
		}

		if (levels.contains("max"))
		{
			maxLevel = levels["max"].get<int32_t>();
		}
		else
		{
			maxLevel = 45;
		}
	}

	if (json.contains("rarity"))
	{
		rarity = magic_enum::enum_cast<ModifierRarity>(json["rarity"].get<std::string>()).value_or(ModifierRarity::Common);
	}
	else
	{
		rarity = ModifierRarity::Common;
	}
}

nlohmann::json nejlika::ModifierNameTemplate::ToJson() const {
	nlohmann::json json;

	json["name"] = name;
	json["type"] = magic_enum::enum_name(type);

	if (lot != 0)
	{
		json["lot"] = lot;
	}

	if (!itemTypes.empty())
	{
		nlohmann::json items;

		for (const auto& itemType : itemTypes)
		{
			items.push_back(magic_enum::enum_name(itemType));
		}

		json["items"] = items;
	}

	if (!modifiers.empty())
	{
		nlohmann::json modifierTemplates;

		for (const auto& modifier : modifiers)
		{
			modifierTemplates.push_back(modifier.ToJson());
		}

		json["modifiers"] = modifierTemplates;
	}

	nlohmann::json levels;

	levels["min"] = minLevel;
	levels["max"] = maxLevel;

	json["levels"] = levels;
	json["rarity"] = magic_enum::enum_name(rarity);

	return json;
}

std::vector<ModifierInstance> nejlika::ModifierNameTemplate::GenerateModifiers(int32_t level) const
{
	std::vector<ModifierInstance> result;

	for (const auto& modifierTemplate : modifiers)
	{
		auto modifiers = modifierTemplate.GenerateModifiers(level);

		result.insert(result.end(), modifiers.begin(), modifiers.end());
	}

	return result;
}
