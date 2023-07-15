#include "CDLookupTable.h"

CDLookupTable::CDLookupTable(void) {

	// First, get the size of the table
	uint32_t size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM lookupTable");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	this->entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM lookupTable");
	while (!tableData.eof()) {
		std::string key = tableData.getStringField("id", "");
		int32_t value = tableData.getIntField("value", -1);

		this->entries.emplace(key, value);
		this->reverseEntries.emplace(value, key);

		tableData.nextRow();
	}

	tableData.finalize();
}

int32_t CDLookupTable::Lookup(const std::string& key) const {
	// If the key starts with 'lego-universe:', parse what comes
	// after the ':' as an integer and return that.
	if (key.find("lego-universe:") == 0) {
		return std::stoi(key.substr(14));
	}

	auto it = this->entries.find(key);
	if (it != this->entries.end()) {
		return it->second;
	}

	return -1;
}

std::vector<std::string> CDLookupTable::ReverseLookup(int32_t value) const {
	auto range = this->reverseEntries.equal_range(value);
	std::vector<std::string> keys;
	for (auto it = range.first; it != range.second; ++it) {
		keys.push_back(it->second);
	}

	return keys;
}

int32_t rose::id(const std::string& key) {
	return CDLookupTable::Instance().Lookup(key);
}
