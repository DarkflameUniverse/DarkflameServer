#include "SQLiteDatabase.h"

std::vector<IContraband::ContrabandItem> SQLiteDatabase::GetContrabandItems() {
	std::vector<ContrabandItem> items;
	auto [_, result] = ExecuteSelect("SELECT * FROM contraband_items ORDER BY lot;");
	for (; !result.eof(); result.nextRow()) {
		items.push_back({ result.getIntField("lot"), result.getStringField("reason"), static_cast<eContrabandAction>(result.getIntField("action")),
			result.getStringField("added_by"), result.getInt64Field("added_at") });
	}
	return items;
}

void SQLiteDatabase::SetContrabandItem(const ContrabandItem& item) {
	ExecuteInsert(
		"INSERT INTO contraband_items (lot, reason, action, added_by, added_at) VALUES (?, ?, ?, ?, ?) "
		"ON CONFLICT(lot) DO UPDATE SET reason = excluded.reason, action = excluded.action, added_by = excluded.added_by, added_at = excluded.added_at;",
		item.lot, item.reason, static_cast<uint32_t>(item.action), item.addedBy, item.addedAt);
}

bool SQLiteDatabase::DeleteContrabandItem(LOT lot) {
	return ExecuteUpdate("DELETE FROM contraband_items WHERE lot = ?;", lot) > 0;
}
