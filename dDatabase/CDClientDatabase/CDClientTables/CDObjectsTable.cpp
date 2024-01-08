#include "CDObjectsTable.h"

void CDObjectsTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Objects");
	while (!tableData.eof()) {
		const uint32_t LOT = tableData.getIntField("id", 0);

		auto& entry = m_Entries[LOT];
		entry.id = LOT;
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

		tableData.nextRow();
	}

	m_Default.id = 0;
}

const CDObjects& CDObjectsTable::GetByID(const uint32_t LOT) {
	const auto& it = m_Entries.find(LOT);
	if (it != m_Entries.end()) {
		return it->second;
	}

	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM Objects WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(LOT));

	auto tableData = query.execQuery();
	if (tableData.eof()) {
		m_Entries.insert(std::make_pair(LOT, m_Default));
		return m_Default;
	}

	// Now get the data
	while (!tableData.eof()) {
		const uint32_t LOT = tableData.getIntField("id", 0);

		auto& entry = m_Entries[LOT];
		entry.id = LOT;
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

		tableData.nextRow();
	}

	tableData.finalize();

	const auto& it2 = m_Entries.find(LOT);
	if (it2 != m_Entries.end()) {
		return it2->second;
	}

	return m_Default;
}
