#include "CDItemComponentTable.h"
#include "GeneralUtils.h"

CDItemComponent CDItemComponentTable::Default = {};

void CDItemComponentTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ItemComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemComponent");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDItemComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.equipLocation = tableData.getStringField("equipLocation", "");
		entry.baseValue = tableData.getIntField("baseValue", -1);
		entry.isKitPiece = tableData.getIntField("isKitPiece", -1) == 1 ? true : false;
		entry.rarity = tableData.getIntField("rarity", 0);
		entry.itemType = tableData.getIntField("itemType", -1);
		entry.itemInfo = tableData.getInt64Field("itemInfo", -1);
		entry.inLootTable = tableData.getIntField("inLootTable", -1) == 1 ? true : false;
		entry.inVendor = tableData.getIntField("inVendor", -1) == 1 ? true : false;
		entry.isUnique = tableData.getIntField("isUnique", -1) == 1 ? true : false;
		entry.isBOP = tableData.getIntField("isBOP", -1) == 1 ? true : false;
		entry.isBOE = tableData.getIntField("isBOE", -1) == 1 ? true : false;
		entry.reqFlagID = tableData.getIntField("reqFlagID", -1);
		entry.reqSpecialtyID = tableData.getIntField("reqSpecialtyID", -1);
		entry.reqSpecRank = tableData.getIntField("reqSpecRank", -1);
		entry.reqAchievementID = tableData.getIntField("reqAchievementID", -1);
		entry.stackSize = tableData.getIntField("stackSize", -1);
		entry.color1 = tableData.getIntField("color1", -1);
		entry.decal = tableData.getIntField("decal", -1);
		entry.offsetGroupID = tableData.getIntField("offsetGroupID", -1);
		entry.buildTypes = tableData.getIntField("buildTypes", -1);
		entry.reqPrecondition = tableData.getStringField("reqPrecondition", "");
		entry.animationFlag = tableData.getIntField("animationFlag", 0);
		entry.equipEffects = tableData.getIntField("equipEffects", -1);
		entry.readyForQA = tableData.getIntField("readyForQA", -1) == 1 ? true : false;
		entry.itemRating = tableData.getIntField("itemRating", -1);
		entry.isTwoHanded = tableData.getIntField("isTwoHanded", -1) == 1 ? true : false;
		entry.minNumRequired = tableData.getIntField("minNumRequired", -1);
		entry.delResIndex = tableData.getIntField("delResIndex", -1);
		entry.currencyLOT = tableData.getIntField("currencyLOT", -1);
		entry.altCurrencyCost = tableData.getIntField("altCurrencyCost", -1);
		entry.subItems = tableData.getStringField("subItems", "");
		UNUSED_COLUMN(entry.audioEventUse = tableData.getStringField("audioEventUse", ""));
		entry.noEquipAnimation = tableData.getIntField("noEquipAnimation", -1) == 1 ? true : false;
		entry.commendationLOT = tableData.getIntField("commendationLOT", -1);
		entry.commendationCost = tableData.getIntField("commendationCost", -1);
		UNUSED_COLUMN(entry.audioEquipMetaEventSet = tableData.getStringField("audioEquipMetaEventSet", ""));
		entry.currencyCosts = tableData.getStringField("currencyCosts", "");
		UNUSED_COLUMN(entry.ingredientInfo = tableData.getStringField("ingredientInfo", ""));
		entry.locStatus = tableData.getIntField("locStatus", -1);
		entry.forgeType = tableData.getIntField("forgeType", -1);
		entry.SellMultiplier = tableData.getFloatField("SellMultiplier", -1.0f);

		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

const CDItemComponent& CDItemComponentTable::GetItemComponentByID(uint32_t skillID) {
	auto& entries = GetEntriesMutable();
	const auto& it = entries.find(skillID);
	if (it != entries.end()) {
		return it->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM ItemComponent WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(skillID));

	auto tableData = query.execQuery();
	if (tableData.eof()) {
		entries.insert(std::make_pair(skillID, Default));
		return Default;
	}

	while (!tableData.eof()) {
		CDItemComponent entry;
		entry.id = tableData.getIntField("id", -1);
		entry.equipLocation = tableData.getStringField("equipLocation", "");
		entry.baseValue = tableData.getIntField("baseValue", -1);
		entry.isKitPiece = tableData.getIntField("isKitPiece", -1) == 1 ? true : false;
		entry.rarity = tableData.getIntField("rarity", 0);
		entry.itemType = tableData.getIntField("itemType", -1);
		entry.itemInfo = tableData.getInt64Field("itemInfo", -1);
		entry.inLootTable = tableData.getIntField("inLootTable", -1) == 1 ? true : false;
		entry.inVendor = tableData.getIntField("inVendor", -1) == 1 ? true : false;
		entry.isUnique = tableData.getIntField("isUnique", -1) == 1 ? true : false;
		entry.isBOP = tableData.getIntField("isBOP", -1) == 1 ? true : false;
		entry.isBOE = tableData.getIntField("isBOE", -1) == 1 ? true : false;
		entry.reqFlagID = tableData.getIntField("reqFlagID", -1);
		entry.reqSpecialtyID = tableData.getIntField("reqSpecialtyID", -1);
		entry.reqSpecRank = tableData.getIntField("reqSpecRank", -1);
		entry.reqAchievementID = tableData.getIntField("reqAchievementID", -1);
		entry.stackSize = tableData.getIntField("stackSize", -1);
		entry.color1 = tableData.getIntField("color1", -1);
		entry.decal = tableData.getIntField("decal", -1);
		entry.offsetGroupID = tableData.getIntField("offsetGroupID", -1);
		entry.buildTypes = tableData.getIntField("buildTypes", -1);
		entry.reqPrecondition = tableData.getStringField("reqPrecondition", "");
		entry.animationFlag = tableData.getIntField("animationFlag", 0);
		entry.equipEffects = tableData.getIntField("equipEffects", -1);
		entry.readyForQA = tableData.getIntField("readyForQA", -1) == 1 ? true : false;
		entry.itemRating = tableData.getIntField("itemRating", -1);
		entry.isTwoHanded = tableData.getIntField("isTwoHanded", -1) == 1 ? true : false;
		entry.minNumRequired = tableData.getIntField("minNumRequired", -1);
		entry.delResIndex = tableData.getIntField("delResIndex", -1);
		entry.currencyLOT = tableData.getIntField("currencyLOT", -1);
		entry.altCurrencyCost = tableData.getIntField("altCurrencyCost", -1);
		entry.subItems = tableData.getStringField("subItems", "");
		UNUSED(entry.audioEventUse = tableData.getStringField("audioEventUse", ""));
		entry.noEquipAnimation = tableData.getIntField("noEquipAnimation", -1) == 1 ? true : false;
		entry.commendationLOT = tableData.getIntField("commendationLOT", -1);
		entry.commendationCost = tableData.getIntField("commendationCost", -1);
		UNUSED(entry.audioEquipMetaEventSet = tableData.getStringField("audioEquipMetaEventSet", ""));
		entry.currencyCosts = tableData.getStringField("currencyCosts", "");
		UNUSED(entry.ingredientInfo = tableData.getStringField("ingredientInfo", ""));
		entry.locStatus = tableData.getIntField("locStatus", -1);
		entry.forgeType = tableData.getIntField("forgeType", -1);
		entry.SellMultiplier = tableData.getFloatField("SellMultiplier", -1.0f);

		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	const auto& it2 = entries.find(skillID);
	if (it2 != entries.end()) {
		return it2->second;
	}

	return Default;
}

std::map<LOT, uint32_t> CDItemComponentTable::ParseCraftingCurrencies(const CDItemComponent& itemComponent) {
	std::map<LOT, uint32_t> currencies = {};

	if (!itemComponent.currencyCosts.empty()) {
		auto currencySplit = GeneralUtils::SplitString(itemComponent.currencyCosts, ',');
		for (const auto& currencyAmount : currencySplit) {
			auto amountSplit = GeneralUtils::SplitString(currencyAmount, ':');

			// Checking for 2 here, not sure what to do when there's more stuff than expected
			if (amountSplit.size() == 2) {
				currencies.insert({
					std::stoull(amountSplit[0]),
					std::stoi(amountSplit[1])
					});
			}
		}
	}

	return currencies;
}

