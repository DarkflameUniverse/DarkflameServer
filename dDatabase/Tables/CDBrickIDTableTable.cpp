#include "CDBrickIDTableTable.h"

//! Constructor
CDBrickIDTableTable::CDBrickIDTableTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM BrickIDTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM BrickIDTable");
	while (!tableData.eof()) {
		CDBrickIDTable entry;
		entry.NDObjectID = tableData.getIntField(0, -1);
		entry.LEGOBrickID = tableData.getIntField(1, -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDBrickIDTableTable::~CDBrickIDTableTable(void) {}

//! Returns the table's name
std::string CDBrickIDTableTable::GetName(void) const {
	return "BrickIDTable";
}

//! Queries the table with a custom "where" clause
std::vector<CDBrickIDTable> CDBrickIDTableTable::Query(std::function<bool(CDBrickIDTable)> predicate) {

	std::vector<CDBrickIDTable> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDBrickIDTable> CDBrickIDTableTable::GetEntries(void) const {
	return this->entries;
}
