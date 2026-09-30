#include "CDObjectsTable.h"
#include "CDFdb.h"
#include "Logger.h"

namespace {
	CDObjects ObjDefault;

	// Fills an entry from a CDServer.sqlite row or an fdb row (CDFdb::RowFields), which read alike
	template<typename Row>
	void ReadEntry(Row& row, CDObjects& entry) {
		entry.name = row.getStringField("name", "");
		UNUSED_COLUMN(entry.placeable = row.getIntField("placeable", -1);)
		entry.type = row.getStringField("type", "");
		UNUSED_COLUMN(entry.description = row.getStringField("description", "");)
		UNUSED_COLUMN(entry.localize = row.getIntField("localize", -1);)
		UNUSED_COLUMN(entry.npcTemplateID = row.getIntField("npcTemplateID", -1);)
		UNUSED_COLUMN(entry.displayName = row.getStringField("displayName", "");)
		entry.interactionDistance = row.getFloatField("interactionDistance", -1.0f);
		UNUSED_COLUMN(entry.nametag = row.getIntField("nametag", -1);)
		UNUSED_COLUMN(entry._internalNotes = row.getStringField("_internalNotes", "");)
		UNUSED_COLUMN(entry.locStatus = row.getIntField("locStatus", -1);)
		UNUSED_COLUMN(entry.gate_version = row.getStringField("gate_version", "");)
		UNUSED_COLUMN(entry.HQ_valid = row.getIntField("HQ_valid", -1);)
	}
};

void CDObjectsTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Objects");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		const uint32_t lot = tableData.getIntField("id", 0);

		auto& entry = entries[lot];
		entry.id = lot;
		ReadEntry(tableData, entry);

		tableData.nextRow();
	}

	ObjDefault.id = 0;
}

bool CDObjectsTable::LoadFromFdb() {
	m_FdbTable = nullptr;
	const auto* table = CDFdb::GetTable("Objects");
	if (!table) return false;

	const auto changed = CDFdb::FindChangedKeys(*table);
	if (!changed) return false;

	// Ids whose rows CDServer.sqlite changes are read from it once and kept; everything else comes from the fdb
	for (const auto id : *changed) LoadFromSqlite(static_cast<uint32_t>(id));
	LOG("Objects: reading from the fdb, %zu ids differ in CDServer.sqlite and are kept in memory", changed->size());

	m_FdbTable = table;
	return true;
}

const CDObjects& CDObjectsTable::LoadFromSqlite(const uint32_t lot) {
	auto& entries = GetEntriesMutable();
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM Objects WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(lot));

	auto tableData = query.execQuery();
	if (tableData.eof()) {
		entries.emplace(lot, ObjDefault);
		return ObjDefault;
	}

	// Now get the data
	while (!tableData.eof()) {
		const uint32_t rowLot = tableData.getIntField("id", 0);

		auto& entry = entries[rowLot];
		entry.id = rowLot;
		ReadEntry(tableData, entry);

		tableData.nextRow();
	}

	tableData.finalize();

	const auto& it = entries.find(lot);
	return it != entries.end() ? it->second : ObjDefault;
}

const CDObjects& CDObjectsTable::GetByID(const uint32_t lot) {
	auto& entries = GetEntriesMutable();
	const auto& it = entries.find(lot);
	if (it != entries.end()) {
		return it->second;
	}

	if (!m_FdbTable) return LoadFromSqlite(lot);

	// Only the objects asked for are kept in memory; the last row of an id wins, as in the SQLite path
	std::optional<CDObjects> found;
	m_FdbTable->ForEachRowWithKey(static_cast<int32_t>(lot), [&](const FdbReader::Row& row) {
		if (!found) found.emplace();
		found->id = lot;
		const CDFdb::RowFields fields(*m_FdbTable, row);
		ReadEntry(fields, *found);
	});
	if (!found) {
		entries.emplace(lot, ObjDefault);
		return ObjDefault;
	}

	return entries.emplace(lot, std::move(*found)).first->second;
}
