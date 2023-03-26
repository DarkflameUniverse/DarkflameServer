#include "CDObjectsTable.h"

//! Constructor
CDObjectsTable::CDObjectsTable(void) {
#ifdef CDCLIENT_CACHE_ALL
	// First, get the size of the table
	unsigned int size = 0;
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
		entry.placeable = tableData.getIntField("placeable", -1);
		entry.type = tableData.getStringField("type", "");
		entry.description = tableData.getStringField("description", "");
		entry.localize = tableData.getIntField("localize", -1);
		entry.npcTemplateID = tableData.getIntField("npcTemplateID", -1);
		entry.displayName = tableData.getStringField("displayName", "");
		entry.interactionDistance = tableData.getFloatField("interactionDistance", -1.0f);
		entry.nametag = tableData.getIntField("nametag", -1);
		entry._internalNotes = tableData.getStringField("_internalNotes", "");
		entry.locStatus = tableData.getIntField("locStatus", -1);
		entry.gate_version = tableData.getStringField("gate_version", "");
		entry.HQ_valid = tableData.getIntField("HQ_valid", -1);

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
#endif

	m_default.id = 0;
}

const CDObjects& CDObjectsTable::GetByID(unsigned int LOT) {
	const auto& it = this->entries.find(LOT);
	if (it != this->entries.end()) {
		return it->second;
	}

#ifndef CDCLIENT_CACHE_ALL
	std::stringstream query;

	query << "SELECT * FROM Objects WHERE id = " << std::to_string(LOT);

	auto tableData = CDClientDatabase::ExecuteQuery(query.str());
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
#endif

	return m_default;
}

