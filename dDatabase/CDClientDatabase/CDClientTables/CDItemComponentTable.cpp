#include "CDItemComponentTable.h"
#include "GeneralUtils.h"
#include "CDFdb.h"
#include "Logger.h"

CDItemComponent CDItemComponentTable::Default = {};

namespace {
	// Fills an entry from a CDServer.sqlite row or an fdb row (CDFdb::RowFields), which read alike
	template<typename Row>
	CDItemComponent ReadEntry(Row& row) {
		CDItemComponent entry;
		entry.id = row.getIntField("id", -1);
		entry.equipLocation = row.getStringField("equipLocation", "");
		entry.baseValue = row.getIntField("baseValue", -1);
		entry.isKitPiece = row.getIntField("isKitPiece", -1) == 1 ? true : false;
		entry.rarity = row.getIntField("rarity", 0);
		entry.itemType = row.getIntField("itemType", -1);
		entry.itemInfo = row.getInt64Field("itemInfo", -1);
		entry.inLootTable = row.getIntField("inLootTable", -1) == 1 ? true : false;
		entry.inVendor = row.getIntField("inVendor", -1) == 1 ? true : false;
		entry.isUnique = row.getIntField("isUnique", -1) == 1 ? true : false;
		entry.isBOP = row.getIntField("isBOP", -1) == 1 ? true : false;
		entry.isBOE = row.getIntField("isBOE", -1) == 1 ? true : false;
		entry.reqFlagID = row.getIntField("reqFlagID", -1);
		entry.reqSpecialtyID = row.getIntField("reqSpecialtyID", -1);
		entry.reqSpecRank = row.getIntField("reqSpecRank", -1);
		entry.reqAchievementID = row.getIntField("reqAchievementID", -1);
		entry.stackSize = row.getIntField("stackSize", -1);
		entry.color1 = row.getIntField("color1", -1);
		entry.decal = row.getIntField("decal", -1);
		entry.offsetGroupID = row.getIntField("offsetGroupID", -1);
		entry.buildTypes = row.getIntField("buildTypes", -1);
		entry.reqPrecondition = row.getStringField("reqPrecondition", "");
		entry.animationFlag = row.getIntField("animationFlag", 0);
		entry.equipEffects = row.getIntField("equipEffects", -1);
		entry.readyForQA = row.getIntField("readyForQA", -1) == 1 ? true : false;
		entry.itemRating = row.getIntField("itemRating", -1);
		entry.isTwoHanded = row.getIntField("isTwoHanded", -1) == 1 ? true : false;
		entry.minNumRequired = row.getIntField("minNumRequired", -1);
		entry.delResIndex = row.getIntField("delResIndex", -1);
		entry.currencyLOT = row.getIntField("currencyLOT", -1);
		entry.altCurrencyCost = row.getIntField("altCurrencyCost", -1);
		entry.subItems = row.getStringField("subItems", "");
		UNUSED_COLUMN(entry.audioEventUse = row.getStringField("audioEventUse", ""));
		entry.noEquipAnimation = row.getIntField("noEquipAnimation", -1) == 1 ? true : false;
		entry.commendationLOT = row.getIntField("commendationLOT", -1);
		entry.commendationCost = row.getIntField("commendationCost", -1);
		UNUSED_COLUMN(entry.audioEquipMetaEventSet = row.getStringField("audioEquipMetaEventSet", ""));
		entry.currencyCosts = row.getStringField("currencyCosts", "");
		UNUSED_COLUMN(entry.ingredientInfo = row.getStringField("ingredientInfo", ""));
		entry.locStatus = row.getIntField("locStatus", -1);
		entry.forgeType = row.getIntField("forgeType", -1);
		entry.SellMultiplier = row.getFloatField("SellMultiplier", -1.0f);
		return entry;
	}
}

void CDItemComponentTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM ItemComponent");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDItemComponent entry = ReadEntry(tableData);
		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

bool CDItemComponentTable::LoadFromFdb() {
	m_FdbTable = nullptr;
	const auto* table = CDFdb::GetTable("ItemComponent");
	if (!table) return false;

	const auto changed = CDFdb::FindChangedKeys(*table);
	if (!changed) return false;

	// Ids whose rows CDServer.sqlite changes are read from it once and kept; everything else comes from the fdb
	for (const auto id : *changed) LoadFromSqlite(static_cast<uint32_t>(id));
	LOG("ItemComponent: reading from the fdb, %zu ids differ in CDServer.sqlite and are kept in memory", changed->size());

	m_FdbTable = table;
	return true;
}

const CDItemComponent& CDItemComponentTable::LoadFromSqlite(uint32_t id) {
	auto& entries = GetEntriesMutable();
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM ItemComponent WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(id));

	auto tableData = query.execQuery();
	if (tableData.eof()) {
		entries.insert(std::make_pair(id, Default));
		return Default;
	}

	while (!tableData.eof()) {
		CDItemComponent entry = ReadEntry(tableData);
		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	const auto& it = entries.find(id);
	return it != entries.end() ? it->second : Default;
}

const CDItemComponent& CDItemComponentTable::GetItemComponentByID(uint32_t skillID) {
	auto& entries = GetEntriesMutable();
	const auto& it = entries.find(skillID);
	if (it != entries.end()) {
		return it->second;
	}

	if (!m_FdbTable) return LoadFromSqlite(skillID);

	// Only the items asked for are kept in memory; the first row of an id wins, as in the SQLite path
	const auto row = m_FdbTable->FindFirst(static_cast<int32_t>(skillID));
	if (!row) {
		entries.insert(std::make_pair(skillID, Default));
		return Default;
	}

	const CDFdb::RowFields fields(*m_FdbTable, *row);
	return entries.insert(std::make_pair(skillID, ReadEntry(fields))).first->second;
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
					GeneralUtils::TryParse<LOT>(amountSplit[0], LOT_NULL),
					GeneralUtils::TryParse<uint32_t>(amountSplit[1], 0)
					});
			}
		}
	}

	return currencies;
}

