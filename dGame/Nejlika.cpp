#include "Nejlika.h"

#include "SlashCommandHandler.h"

#include <InventoryComponent.h>
#include <Item.h>
#include <ChatPackets.h>
#include <Amf3.h>
#include <iomanip>

void nejlika::Initalize()
{
	Command itemDescriptionCommand{
		.help = "Special UI command, does nothing when used in chat.",
		.info = "Special UI command, does nothing when used in chat.",
		.aliases = {"d"},
		.handle = ItemDescription,
		.requiredLevel = eGameMasterLevel::CIVILIAN
	};
	SlashCommandHandler::RegisterCommand(itemDescriptionCommand);
}

void nejlika::ItemDescription(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;
	
	auto requestId = GeneralUtils::TryParse<int32_t>(splitArgs[0]);
	
	if (!requestId.has_value()) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid item ID.");
		return;
	}

	auto itemId = GeneralUtils::TryParse<LWOOBJID>(splitArgs[1]);

	if (!itemId.has_value()) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid item ID.");
		return;
	}

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (!inventoryComponent) {
		return;
	}

	auto* item = inventoryComponent->FindItemById(itemId.value());

	if (!item) {
		ChatPackets::SendSystemMessage(sysAddr, u"Item not found.");
		return;
	}
	
	const auto& itemData = item->GetConfig();

	LDFBaseData* modifiersData = nullptr;

	for (const auto& data : itemData) {
		if (data->GetKey() == u"modifiers") {
			modifiersData = data;
			break;
		}
	}

	if (!modifiersData) {
		return;
	}

	const auto modifiersStr = dynamic_cast<LDFData<std::string>*>(modifiersData)->GetValueAsString();

	if (modifiersStr.empty()) {
		return;
	}

	std::stringstream name;
	std::stringstream desc;

	auto parts = GeneralUtils::SplitString(modifiersStr, '&');

	if (parts.size() != 2) {
		return;
	}

	auto names = GeneralUtils::SplitString(parts[0], ';');

	std::vector<ItemName> itemNames;

	for (const auto& name : names) {
		ItemName itemName(name);

		itemNames.push_back(itemName);
	}

	name << ItemName::GenerateHtmlString(itemNames) << "\n";

	auto modifiers = GeneralUtils::SplitString(parts[1], ';');

	for (const auto& modifier : modifiers) {
		ItemModifier itemModifier(modifier);

		desc << itemModifier.GenerateHtmlString() << "\n";
	}

	std::stringstream messageName;
	messageName << "desc" << requestId.value();

	AMFArrayValue amfArgs;

	amfArgs.Insert("t", true);
	amfArgs.Insert("d", desc.str());
	amfArgs.Insert("n", name.str());

	GameMessages::SendUIMessageServerToSingleClient(entity, sysAddr, messageName.str(), amfArgs);
}

nejlika::ItemModifier::ItemModifier(const std::string& config) {
	auto splitConfig = GeneralUtils::SplitString(config, ',');

	if (splitConfig.size() != 3) {
		return;
	}

	type = static_cast<ItemModifierType>(GeneralUtils::TryParse<uint8_t>(splitConfig[0]).value());
	value = GeneralUtils::TryParse<float>(splitConfig[1]).value();
	isPercentage = GeneralUtils::TryParse<bool>(splitConfig[2]).value();
}

std::string nejlika::ItemModifier::ToString() const {
	std::stringstream ss;
	ss << static_cast<uint8_t>(type) << ',' << value << ',' << isPercentage;
	return ss.str();
}

std::string nejlika::ItemModifier::GenerateHtmlString() const {
	// "<font color=\"#38B6FF\">Physical: +20%</font>\n..."

	std::stringstream ss;
	
	ss << "<font color=\"";

	/*
	 * Health: #750000
	 * Armor: #525252
	 * Imagination: #0077FF
	 * Slashing: #666666
	 * Piercing: #4f4f4f
	 * Bludgeoning: #e84646
	 * Fire: #ff0000
	 * Cold: #94d0f2
	 * Lightning: #00a2ff
	 * Corruption: #3d00ad
	 * Psychic: #4b0161
	 */
	static const std::unordered_map<ItemModifierType, std::string> colorMap = {
		{ItemModifierType::Health, "#750000"},
		{ItemModifierType::Armor, "#525252"},
		{ItemModifierType::Imagination, "#0077FF"},
		{ItemModifierType::Slashing, "#666666"},
		{ItemModifierType::Piercing, "#4f4f4f"},
		{ItemModifierType::Bludgeoning, "#e84646"},
		{ItemModifierType::Fire, "#ff0000"},
		{ItemModifierType::Cold, "#94d0f2"},
		{ItemModifierType::Lightning, "#00a2ff"},
		{ItemModifierType::Corruption, "#3d00ad"},
		{ItemModifierType::Psychic, "#4b0161"}
	};

	static const std::unordered_map<ItemModifierType, std::string> namesMap = {
		{ItemModifierType::Health, "Health"},
		{ItemModifierType::Armor, "Armor"},
		{ItemModifierType::Imagination, "Imagination"},
		{ItemModifierType::Slashing, "Slashing"},
		{ItemModifierType::Piercing, "Piercing"},
		{ItemModifierType::Bludgeoning, "Bludgeoning"},
		{ItemModifierType::Fire, "Fire"},
		{ItemModifierType::Cold, "Cold"},
		{ItemModifierType::Lightning, "Lightning"},
		{ItemModifierType::Corruption, "Corruption"},
		{ItemModifierType::Psychic, "Psychic"}
	};

	const auto color = colorMap.find(type);

	if (color != colorMap.end()) {
		ss << color->second;
	} else {
		ss << "#FFFFFF";
	}

	ss << "\">";

	const auto name = namesMap.find(type);

	if (name != namesMap.end()) {
		ss << name->second;
	} else {
		ss << "Unknown";
	}

	ss << ": ";
	
	if (value > 0) {
		ss << "+";
	}
	else if (value < 0) {
		ss << "-";
	}

	// Only show 2 decimal places
	ss << std::fixed << std::setprecision(2) << std::abs(value);

	if (isPercentage) {
		ss << "%";
	}

	ss << "</font>";
	
	return ss.str();
}

nejlika::ItemName::ItemName(const std::string& config) {
	auto splitConfig = GeneralUtils::SplitString(config, ',');

	if (splitConfig.size() != 3) {
		return;
	}

	type = static_cast<ItemNameType>(GeneralUtils::TryParse<uint8_t>(splitConfig[0]).value());
	name = splitConfig[1];
	prefix = GeneralUtils::TryParse<bool>(splitConfig[2]).value();
}

std::string nejlika::ItemName::ToString() const {
	std::stringstream ss;
	ss << static_cast<uint8_t>(type) << ',' << name << ',' << prefix;
	return ss.str();
}

std::string nejlika::ItemName::GenerateHtmlString() const {
	std::stringstream ss;

	ss << "<font color=\"";

	/*
	 * Common: #FFFFFF
	 * Uncommon: #00FF00
	 * Rare: #0077FF
	 * Epic: #FF00FF
	 * Legendary: #FF7700
	 * Relic: #FFC391
	 */

	static const std::unordered_map<ItemNameType, std::string> colorMap = {
		{ItemNameType::Common, "#FFFFFF"},
		{ItemNameType::Uncommon, "#00FF00"},
		{ItemNameType::Rare, "#0077FF"},
		{ItemNameType::Epic, "#FF00FF"},
		{ItemNameType::Legendary, "#FF7700"},
		{ItemNameType::Relic, "#FFC391"}
	};

	const auto color = colorMap.find(type);

	if (color != colorMap.end()) {
		ss << color->second;
	} else {
		ss << "#FFFFFF";
	}

	ss << "\">";

	ss << name;

	ss << "</font>";

	return ss.str();
}

std::string nejlika::ItemName::GenerateHtmlString(const std::vector<ItemName>& names) {
    // Prefix-1 Prefix-2 NAME Suffix-1 Suffix-2
	std::stringstream ss;

	for (const auto& name : names) {
		if (name.prefix) {
			ss << name.GenerateHtmlString() << "\n";
		}
	}

	ss << "<font color=\"#56B555\">NAME</font>";

	for (const auto& name : names) {
		if (!name.prefix && !name.name.empty()) {
			ss << name.GenerateHtmlString() << "\n";
		}
	}

	// Remove the last newline
	auto str = ss.str();

	if (!str.empty() && str.back() == '\n') {
		str.pop_back();
	}

	return str;
}


