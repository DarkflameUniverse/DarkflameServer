#include "CDCurrencyTableTable.h"

//! Constructor
CDCurrencyTableTable::CDCurrencyTableTable(void) {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM CurrencyTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM CurrencyTable");
	while (!tableData.eof()) {
		CDCurrencyTable entry;
		entry.currencyIndex = tableData.getIntField(0, -1);
		entry.npcminlevel = tableData.getIntField(1, -1);
		entry.minvalue = tableData.getIntField(2, -1);
		entry.maxvalue = tableData.getIntField(3, -1);
		entry.id = tableData.getIntField(4, -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

//! Destructor
CDCurrencyTableTable::~CDCurrencyTableTable(void) {}

//! Returns the table's name
std::string CDCurrencyTableTable::GetName(void) const {
	return "CurrencyTable";
}

//! Queries the table with a custom "where" clause
std::vector<CDCurrencyTable> CDCurrencyTableTable::Query(std::function<bool(CDCurrencyTable)> predicate) {

	std::vector<CDCurrencyTable> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

//! Gets all the entries in the table
std::vector<CDCurrencyTable> CDCurrencyTableTable::GetEntries(void) const {
	return this->entries;
}
