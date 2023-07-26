#include "CDVendorComponentTable.h"

void CDVendorComponentTable::LoadValuesFromDatabase() {
	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM VendorComponent");
	while (!tableData.eof()) {
		CDVendorComponent entry;
		uint32_t id = tableData.getIntField("id", -1);
		entry.buyScalar = tableData.getFloatField("buyScalar", -1.0f);
		entry.sellScalar = tableData.getFloatField("sellScalar", -1.0f);
		entry.refreshTimeSeconds = tableData.getFloatField("refreshTimeSeconds", -1.0f);
		entry.LootMatrixIndex = tableData.getIntField("LootMatrixIndex", -1);

		this->entries.insert_or_assign(id, entry);
		tableData.nextRow();
	}
}

const std::optional<CDVendorComponent> CDVendorComponentTable::Query(uint32_t id) {
	const auto& iter = entries.find(id);
	return iter != entries.end() ? std::make_optional(iter->second) : std::nullopt;
}
