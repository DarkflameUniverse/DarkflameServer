#include "MySQLDatabase.h"

std::vector<IContraband::ContrabandItem> MySQLDatabase::GetContrabandItems() {
	std::vector<ContrabandItem> items;
	auto result = ExecuteSelect("SELECT * FROM contraband_items ORDER BY lot;");
	while (result->next()) {
		items.push_back({ result->getInt("lot"), result->getString("reason").c_str(), static_cast<eContrabandAction>(result->getUInt("action")),
			result->getString("added_by").c_str(), result->getInt64("added_at") });
	}
	return items;
}

void MySQLDatabase::SetContrabandItem(const ContrabandItem& item) {
	ExecuteInsert(
		"INSERT INTO contraband_items (lot, reason, action, added_by, added_at) VALUES (?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE reason = VALUES(reason), action = VALUES(action), added_by = VALUES(added_by), added_at = VALUES(added_at);",
		item.lot, item.reason, static_cast<uint32_t>(item.action), item.addedBy, item.addedAt);
}

bool MySQLDatabase::DeleteContrabandItem(LOT lot) {
	return ExecuteUpdate("DELETE FROM contraband_items WHERE lot = ?;", lot) > 0;
}
