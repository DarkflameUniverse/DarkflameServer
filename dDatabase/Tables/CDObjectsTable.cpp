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
		entry.id = tableData.getIntField(0, -1);
		entry.name = tableData.getStringField(1, "");
		entry.placeable = tableData.getIntField(2, -1);
		entry.type = tableData.getStringField(3, "");
		entry.description = tableData.getStringField(4, "");
		entry.localize = tableData.getIntField(5, -1);
		entry.npcTemplateID = tableData.getIntField(6, -1);
		entry.displayName = tableData.getStringField(7, "");
		entry.interactionDistance = tableData.getFloatField(8, -1.0f);
		entry.nametag = tableData.getIntField(9, -1);
		entry._internalNotes = tableData.getStringField(10, "");
		entry.locStatus = tableData.getIntField(11, -1);
		entry.gate_version = tableData.getStringField(12, "");
		entry.HQ_valid = tableData.getIntField(13, -1);

		this->entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
#endif

	m_default.id = 0;
}

//! Destructor
CDObjectsTable::~CDObjectsTable(void) {}

//! Returns the table's name
std::string CDObjectsTable::GetName(void) const {
	return "Objects";
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
		entry.id = tableData.getIntField(0, -1);
		entry.name = tableData.getStringField(1, "");
		UNUSED(entry.placeable = tableData.getIntField(2, -1));
		entry.type = tableData.getStringField(3, "");
		UNUSED(ntry.description = tableData.getStringField(4, ""));
		UNUSED(entry.localize = tableData.getIntField(5, -1));
		UNUSED(entry.npcTemplateID = tableData.getIntField(6, -1));
		UNUSED(entry.displayName = tableData.getStringField(7, ""));
		entry.interactionDistance = tableData.getFloatField(8, -1.0f);
		UNUSED(entry.nametag = tableData.getIntField(9, -1));
		UNUSED(entry._internalNotes = tableData.getStringField(10, ""));
		UNUSED(entry.locStatus = tableData.getIntField(11, -1));
		UNUSED(entry.gate_version = tableData.getStringField(12, ""));
		UNUSED(entry.HQ_valid = tableData.getIntField(13, -1));

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
