#include "CDDeletionRestrictionsTable.h"
#include "GeneralUtils.h"
#include "eDeletionRestrictionsCheckType.h"

CDDeletionRestriction CDDeletionRestrictionsTable::Default = {
	.id = 0,
	.restricted = false,
	.ids = {},
	.checkType = eDeletionRestrictionsCheckType::MAX
};

void CDDeletionRestrictionsTable::LoadValuesFromDatabase() {
	auto& entries = GetEntriesMutable();

	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM DeletionRestrictions");
	while (!tableData.eof()) {
		CDDeletionRestriction entry;
		entry.id = tableData.getIntField("id", -1);
		if (entry.id == -1) continue;
		entry.restricted = tableData.getIntField("restricted", -1);
		const std::string raw_ids = tableData.getStringField("ids", "");
		if (!raw_ids.empty()) {
			for (const auto& idstr : GeneralUtils::SplitString(raw_ids, ',')) {
				if (!idstr.empty()) {
					const auto id = GeneralUtils::TryParse<int32_t>(idstr);
					if (id) entry.ids.push_back(id.value());
				} 
			}
		}
		entry.checkType = static_cast<eDeletionRestrictionsCheckType>(tableData.getIntField("checkType", 6)); // MAX

		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

}

const CDDeletionRestriction& CDDeletionRestrictionsTable::GetByID(uint32_t id) {
	auto& entries = GetEntries();
	const auto& it = entries.find(id);
	if (it != entries.end()) {
		return it->second;
	}
	return Default;
}
