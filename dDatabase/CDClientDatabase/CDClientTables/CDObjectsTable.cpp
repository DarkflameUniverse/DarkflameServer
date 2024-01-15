#include "CDObjectsTable.h"

void CDObjectsTable::LoadValuesFromDatabase() {
	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM Objects");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Objects");
	while (!tableData.eof()) {
		CDObjects entry;
		entry.id = tableData.getIntField("id", -1);
		entry.name = tableData.getStringField("name", "");
		UNUSED_COLUMN(entry.placeable = tableData.getIntField("placeable", -1);)
		entry.type = tableData.getStringField("type", "");
		UNUSED_COLUMN(entry.description = tableData.getStringField("description", "");)
		UNUSED_COLUMN(entry.localize = tableData.getIntField("localize", -1);)
		UNUSED_COLUMN(entry.npcTemplateID = tableData.getIntField("npcTemplateID", -1);)
		UNUSED_COLUMN(entry.displayName = tableData.getStringField("displayName", "");)
		entry.interactionDistance = tableData.getFloatField("interactionDistance", -1.0f);
		UNUSED_COLUMN(entry.nametag = tableData.getIntField("nametag", -1);)
		UNUSED_COLUMN(entry._internalNotes = tableData.getStringField("_internalNotes", "");)
		UNUSED_COLUMN(entry.locStatus = tableData.getIntField("locStatus", -1);)
		UNUSED_COLUMN(entry.gate_version = tableData.getStringField("gate_version", "");)
		UNUSED_COLUMN(entry.HQ_valid = tableData.getIntField("HQ_valid", -1);)

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();

	m_default.id = 0;
}

const CDObjects& CDObjectsTable::GetByID(uint32_t LOT) {
	const auto& it = this->entries.find(LOT);
	if (it != this->entries.end()) {
		return it->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM Objects WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(LOT));

	auto tableData = query.execQuery();
	if (tableData.eof()) {
		this->entries.insert(std::make_pair(LOT, m_default));
		return m_default;
	}

	// Now get the data
	while (!tableData.eof()) {
		CDObjects entry;
		entry.id = tableData.getIntField("id", -1);
		entry.name = tableData.getStringField("name", "");
		UNUSED(entry.placeable = tableData.getIntField("placeable", -1));
		entry.type = tableData.getStringField("type", "");
		UNUSED(ntry.description = tableData.getStringField(4, ""));
		UNUSED(entry.localize = tableData.getIntField("localize", -1));
		UNUSED(entry.npcTemplateID = tableData.getIntField("npcTemplateID", -1));
		UNUSED(entry.displayName = tableData.getStringField("displayName", ""));
		entry.interactionDistance = tableData.getFloatField("interactionDistance", -1.0f);
		UNUSED(entry.nametag = tableData.getIntField("nametag", -1));
		UNUSED(entry._internalNotes = tableData.getStringField("_internalNotes", ""));
		UNUSED(entry.locStatus = tableData.getIntField("locStatus", -1));
		UNUSED(entry.gate_version = tableData.getStringField("gate_version", ""));
		UNUSED(entry.HQ_valid = tableData.getIntField("HQ_valid", -1));

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();

	const auto& it2 = entries.find(LOT);
	if (it2 != entries.end()) {
		return it2->second;
	}

	return m_default;
}

