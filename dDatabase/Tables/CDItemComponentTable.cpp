#include "CDItemComponentTable.h"
#include "GeneralUtils.h"

CDItemComponent CDItemComponentTable::Default = {};

//! Constructor
CDItemComponentTable::CDItemComponentTable(void) {
	Default = CDItemComponent();

#ifdef CDCLIENT_CACHE_ALL
	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM ItemComponent");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemComponent");
	while (!tableData.eof()) {
		CDItemComponent entry;
		entry.id = tableData.getIntField(0, -1);
		entry.equipLocation = tableData.getStringField(1, "");
		entry.baseValue = tableData.getIntField(2, -1);
		entry.isKitPiece = tableData.getIntField(3, -1) == 1 ? true : false;
		entry.rarity = tableData.getIntField(4, 0);
		entry.itemType = tableData.getIntField(5, -1);
		entry.itemInfo = tableData.getInt64Field(6, -1);
		entry.inLootTable = tableData.getIntField(7, -1) == 1 ? true : false;
		entry.inVendor = tableData.getIntField(8, -1) == 1 ? true : false;
		entry.isUnique = tableData.getIntField(9, -1) == 1 ? true : false;
		entry.isBOP = tableData.getIntField(10, -1) == 1 ? true : false;
		entry.isBOE = tableData.getIntField(11, -1) == 1 ? true : false;
		entry.reqFlagID = tableData.getIntField(12, -1);
		entry.reqSpecialtyID = tableData.getIntField(13, -1);
		entry.reqSpecRank = tableData.getIntField(14, -1);
		entry.reqAchievementID = tableData.getIntField(15, -1);
		entry.stackSize = tableData.getIntField(16, -1);
		entry.color1 = tableData.getIntField(17, -1);
		entry.decal = tableData.getIntField(18, -1);
		entry.offsetGroupID = tableData.getIntField(19, -1);
		entry.buildTypes = tableData.getIntField(20, -1);
		entry.reqPrecondition = tableData.getStringField(21, "");
		entry.animationFlag = tableData.getIntField(22, 0);
		entry.equipEffects = tableData.getIntField(23, -1);
		entry.readyForQA = tableData.getIntField(24, -1) == 1 ? true : false;
		entry.itemRating = tableData.getIntField(25, -1);
		entry.isTwoHanded = tableData.getIntField(26, -1) == 1 ? true : false;
		entry.minNumRequired = tableData.getIntField(27, -1);
		entry.delResIndex = tableData.getIntField(28, -1);
		entry.currencyLOT = tableData.getIntField(29, -1);
		entry.altCurrencyCost = tableData.getIntField(30, -1);
		entry.subItems = tableData.getStringField(31, "");
		entry.audioEventUse = tableData.getStringField(32, "");
		entry.noEquipAnimation = tableData.getIntField(33, -1) == 1 ? true : false;
		entry.commendationLOT = tableData.getIntField(34, -1);
		entry.commendationCost = tableData.getIntField(35, -1);
		entry.audioEquipMetaEventSet = tableData.getStringField(36, "");
		entry.currencyCosts = tableData.getStringField(37, "");
		entry.ingredientInfo = tableData.getStringField(38, "");
		entry.locStatus = tableData.getIntField(39, -1);
		entry.forgeType = tableData.getIntField(40, -1);
		entry.SellMultiplier = tableData.getFloatField(41, -1.0f);

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
#endif
}

//! Destructor
CDItemComponentTable::~CDItemComponentTable(void) {}

//! Returns the table's name
std::string CDItemComponentTable::GetName(void) const {
	return "ItemComponent";
}

const CDItemComponent& CDItemComponentTable::GetItemComponentByID(unsigned int skillID) {
	const auto& it = this->entries.find(skillID);
	if (it != this->entries.end()) {
		return it->second;
	}

#ifndef CDCLIENT_CACHE_ALL
	std::stringstream query;

	query << "SELECT * FROM ItemComponent WHERE id = " << std::to_string(skillID);

	auto tableData = CDClientDatabase::ExecuteQuery(query.str());
	if (tableData.eof()) {
		entries.insert(std::make_pair(skillID, Default));
		return Default;
	}

	while (!tableData.eof()) {
		CDItemComponent entry;
		entry.id = tableData.getIntField(0, -1);
		entry.equipLocation = tableData.getStringField(1, "");
		entry.baseValue = tableData.getIntField(2, -1);
		entry.isKitPiece = tableData.getIntField(3, -1) == 1 ? true : false;
		entry.rarity = tableData.getIntField(4, 0);
		entry.itemType = tableData.getIntField(5, -1);
		entry.itemInfo = tableData.getInt64Field(6, -1);
		entry.inLootTable = tableData.getIntField(7, -1) == 1 ? true : false;
		entry.inVendor = tableData.getIntField(8, -1) == 1 ? true : false;
		entry.isUnique = tableData.getIntField(9, -1) == 1 ? true : false;
		entry.isBOP = tableData.getIntField(10, -1) == 1 ? true : false;
		entry.isBOE = tableData.getIntField(11, -1) == 1 ? true : false;
		entry.reqFlagID = tableData.getIntField(12, -1);
		entry.reqSpecialtyID = tableData.getIntField(13, -1);
		entry.reqSpecRank = tableData.getIntField(14, -1);
		entry.reqAchievementID = tableData.getIntField(15, -1);
		entry.stackSize = tableData.getIntField(16, -1);
		entry.color1 = tableData.getIntField(17, -1);
		entry.decal = tableData.getIntField(18, -1);
		entry.offsetGroupID = tableData.getIntField(19, -1);
		entry.buildTypes = tableData.getIntField(20, -1);
		entry.reqPrecondition = tableData.getStringField(21, "");
		entry.animationFlag = tableData.getIntField(22, 0);
		entry.equipEffects = tableData.getIntField(23, -1);
		entry.readyForQA = tableData.getIntField(24, -1) == 1 ? true : false;
		entry.itemRating = tableData.getIntField(25, -1);
		entry.isTwoHanded = tableData.getIntField(26, -1) == 1 ? true : false;
		entry.minNumRequired = tableData.getIntField(27, -1);
		entry.delResIndex = tableData.getIntField(28, -1);
		entry.currencyLOT = tableData.getIntField(29, -1);
		entry.altCurrencyCost = tableData.getIntField(30, -1);
		entry.subItems = tableData.getStringField(31, "");
		UNUSED(entry.audioEventUse = tableData.getStringField(32, ""));
		entry.noEquipAnimation = tableData.getIntField(33, -1) == 1 ? true : false;
		entry.commendationLOT = tableData.getIntField(34, -1);
		entry.commendationCost = tableData.getIntField(35, -1);
		UNUSED(entry.audioEquipMetaEventSet = tableData.getStringField(36, ""));
		entry.currencyCosts = tableData.getStringField(37, "");
		UNUSED(entry.ingredientInfo = tableData.getStringField(38, ""));
		entry.locStatus = tableData.getIntField(39, -1);
		entry.forgeType = tableData.getIntField(40, -1);
		entry.SellMultiplier = tableData.getFloatField(41, -1.0f);

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	const auto& it2 = this->entries.find(skillID);
	if (it2 != this->entries.end()) {
		return it2->second;
	}
#endif

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
