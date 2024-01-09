#include "CDCurrencyTableTable.h"

//! Constructor
void CDCurrencyTableTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	uint32_t size = 0;
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
		entry.currencyIndex = tableData.getIntField("currencyIndex", -1);
		entry.npcminlevel = tableData.getIntField("npcminlevel", -1);
		entry.minvalue = tableData.getIntField("minvalue", -1);
		entry.maxvalue = tableData.getIntField("maxvalue", -1);
		entry.id = tableData.getIntField("id", -1);

		this->entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDCurrencyTable> CDCurrencyTableTable::Query(std::function<bool(CDCurrencyTable)> predicate) {

	std::vector<CDCurrencyTable> data = cpplinq::from(this->entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDCurrencyTable>& CDCurrencyTableTable::GetEntries() const {
	return this->entries;
}

