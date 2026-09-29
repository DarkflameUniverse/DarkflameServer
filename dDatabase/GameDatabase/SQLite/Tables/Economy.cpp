#include "SQLiteDatabase.h"

namespace {
	// Staff (GM 3+) are excluded from player reports, matching NexusDashboard. Deleted characters still count.
	constexpr const char* STAFF_FILTER =
		" LEFT JOIN charinfo c ON c.id = f.character_id LEFT JOIN accounts a ON a.id = c.account_id WHERE COALESCE(a.gm_level, 0) < 3 AND ";
	constexpr const char* NO_FILTER = " WHERE ";
}

void SQLiteDatabase::RecordEconomy(const std::vector<CurrencyFlow>& currency, const std::vector<UScoreFlow>& uscore,
	const std::vector<ItemFlow>& items, const std::vector<ItemTransfer>& transfers, const std::vector<MapEvent>& mapEvents,
	const std::vector<PlayerStat>& stats) {
	if (currency.empty() && uscore.empty() && items.empty() && transfers.empty() && mapEvents.empty() && stats.empty()) return;

	DatabaseTransaction transaction(*this);
	for (const auto& flow : currency) {
		ExecuteInsert(
			"INSERT INTO economy_currency_daily (day, character_id, source, gained, spent) VALUES (?, ?, ?, ?, ?) "
			"ON CONFLICT(day, character_id, source) DO UPDATE SET gained = gained + excluded.gained, spent = spent + excluded.spent;",
			flow.day, flow.characterId, flow.source, flow.gained, flow.spent);
	}
	for (const auto& flow : uscore) {
		ExecuteInsert(
			"INSERT INTO economy_uscore_daily (day, character_id, source, gained, lost) VALUES (?, ?, ?, ?, ?) "
			"ON CONFLICT(day, character_id, source) DO UPDATE SET gained = gained + excluded.gained, lost = lost + excluded.lost;",
			flow.day, flow.characterId, flow.source, flow.gained, flow.lost);
	}
	for (const auto& flow : items) {
		ExecuteInsert(
			"INSERT INTO economy_items_daily (day, lot, source, gm, created, destroyed) VALUES (?, ?, ?, ?, ?, ?) "
			"ON CONFLICT(day, lot, source, gm) DO UPDATE SET created = created + excluded.created, destroyed = destroyed + excluded.destroyed;",
			flow.day, flow.lot, flow.source, flow.gm ? 1 : 0, flow.created, flow.destroyed);
	}
	for (const auto& transfer : transfers) {
		ExecuteInsert(
			"INSERT INTO economy_transfers (time, method, item_id, new_item_id, lot, count, coins, from_character, to_character, zone, merged) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
			transfer.time, static_cast<uint32_t>(transfer.method), transfer.itemId, transfer.newItemId, transfer.lot,
			transfer.count, transfer.coins, transfer.fromCharacter, transfer.toCharacter, transfer.zone, transfer.merged ? 1 : 0);
	}
	for (const auto& event : mapEvents) {
		ExecuteInsert(
			"INSERT INTO map_events_daily (day, zone, clone_id, kind, lot, cell_x, cell_z, events, quantity) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
			"ON CONFLICT(zone, kind, day, clone_id, lot, cell_x, cell_z) DO UPDATE SET events = events + excluded.events, quantity = quantity + excluded.quantity;",
			event.day, event.zone, event.clone, static_cast<uint32_t>(event.kind), event.lot, event.cellX, event.cellZ, event.events, event.quantity);
	}
	for (const auto& stat : stats) {
		ExecuteInsert(
			"INSERT INTO player_stats_daily (day, zone, clone_id, stat, gm, amount) VALUES (?, ?, ?, ?, ?, ?) "
			"ON CONFLICT(day, zone, clone_id, stat, gm) DO UPDATE SET amount = amount + excluded.amount;",
			stat.day, stat.zone, stat.clone, stat.stat, stat.gm ? 1 : 0, stat.amount);
	}
	transaction.Commit();
}

nlohmann::json SQLiteDatabase::GetCurrencyFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) {
	auto [_, result] = ExecuteSelect(std::string("SELECT f.day, f.source, SUM(f.gained) AS gained, SUM(f.spent) AS spent FROM economy_currency_daily f") +
		(excludeStaff ? STAFF_FILTER : NO_FILTER) + "f.day BETWEEN ? AND ? GROUP BY f.day, f.source ORDER BY f.day;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (!result.eof()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"source", result.getIntField("source")},
			{"gained", result.getInt64Field("gained")}, {"spent", result.getInt64Field("spent")} });
		result.nextRow();
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetUScoreFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) {
	auto [_, result] = ExecuteSelect(std::string("SELECT f.day, f.source, SUM(f.gained) AS gained, SUM(f.lost) AS lost FROM economy_uscore_daily f") +
		(excludeStaff ? STAFF_FILTER : NO_FILTER) + "f.day BETWEEN ? AND ? GROUP BY f.day, f.source ORDER BY f.day;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (!result.eof()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"source", result.getIntField("source")},
			{"gained", result.getInt64Field("gained")}, {"lost", result.getInt64Field("lost")} });
		result.nextRow();
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetItemFlows(uint32_t fromDay, uint32_t toDay, LOT lot, bool excludeStaff) {
	const std::string query = std::string("SELECT day, source, SUM(created) AS created, SUM(destroyed) AS destroyed FROM economy_items_daily "
		"WHERE day BETWEEN ? AND ?") + (excludeStaff ? " AND gm = 0" : "") + (lot > 0 ? " AND lot = ?" : "") + " GROUP BY day, source ORDER BY day;";
	auto [_, result] = lot > 0 ? ExecuteSelect(query, fromDay, toDay, lot) : ExecuteSelect(query, fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (!result.eof()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"source", result.getIntField("source")},
			{"created", result.getInt64Field("created")}, {"destroyed", result.getInt64Field("destroyed")} });
		result.nextRow();
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetTopEarners(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) {
	// Trades and mail move coins between players rather than creating them, so they are left out of "income"
	auto [_, result] = ExecuteSelect(std::string(
		"SELECT f.character_id, COALESCE(c.name, '') AS name, SUM(f.gained) AS gained, SUM(f.spent) AS spent FROM economy_currency_daily f "
		"LEFT JOIN charinfo c ON c.id = f.character_id LEFT JOIN accounts a ON a.id = c.account_id "
		"WHERE f.day BETWEEN ? AND ? AND f.source NOT IN (3, 6)") + (excludeStaff ? " AND COALESCE(a.gm_level, 0) < 3" : "") +
		" GROUP BY f.character_id ORDER BY gained DESC LIMIT ?;", fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (!result.eof()) {
		rows.push_back({ {"character_id", std::to_string(result.getInt64Field("character_id"))}, {"name", result.getStringField("name")},
			{"gained", result.getInt64Field("gained")}, {"spent", result.getInt64Field("spent")} });
		result.nextRow();
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetTopItems(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) {
	auto [_, result] = ExecuteSelect(std::string(
		"SELECT lot, SUM(created) AS created, SUM(destroyed) AS destroyed FROM economy_items_daily WHERE day BETWEEN ? AND ?") +
		(excludeStaff ? " AND gm = 0" : "") + " GROUP BY lot ORDER BY created DESC LIMIT ?;", fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (!result.eof()) {
		rows.push_back({ {"lot", result.getIntField("lot")}, {"created", result.getInt64Field("created")}, {"destroyed", result.getInt64Field("destroyed")} });
		result.nextRow();
	}
	return rows;
}

namespace {
	nlohmann::json TransferRow(CppSQLite3Query& result) {
		return {
			{"id", result.getInt64Field("id")},
			{"time", result.getInt64Field("time")},
			{"method", result.getIntField("method")},
			{"item_id", std::to_string(result.getInt64Field("item_id"))},
			{"new_item_id", std::to_string(result.getInt64Field("new_item_id"))},
			{"lot", result.getIntField("lot")},
			{"count", result.getIntField("count")},
			{"coins", result.getInt64Field("coins")},
			{"from_character", std::to_string(result.getInt64Field("from_character"))},
			{"from_name", result.fieldIsNull("from_name") ? "" : result.getStringField("from_name")},
			{"to_character", std::to_string(result.getInt64Field("to_character"))},
			{"to_name", result.fieldIsNull("to_name") ? "" : result.getStringField("to_name")},
			{"zone", result.getIntField("zone")},
			{"merged", result.fieldIsNull("merged") ? nlohmann::json() : nlohmann::json(result.getIntField("merged") != 0)}
		};
	}

	constexpr const char* TRANSFER_SELECT =
		"SELECT t.*, cf.name AS from_name, ct.name AS to_name FROM economy_transfers t "
		"LEFT JOIN charinfo cf ON cf.id = t.from_character LEFT JOIN charinfo ct ON ct.id = t.to_character";
}

nlohmann::json SQLiteDatabase::GetTransfers(uint32_t start, uint32_t length, LWOOBJID characterId, LOT lot) {
	std::string where = " WHERE t.method <> " + std::to_string(static_cast<uint32_t>(eTransferMethod::INVENTORY_MOVE));
	if (characterId != 0) where += " AND (t.from_character = ? OR t.to_character = ?)";
	if (lot > 0) where += " AND t.lot = ?";

	const auto countQuery = "SELECT COUNT(*) AS count FROM economy_transfers t" + where + ";";
	const auto query = std::string(TRANSFER_SELECT) + where + " ORDER BY t.id DESC LIMIT ? OFFSET ?;";
	auto run = [&](const std::string& sql, auto... extra) {
		if (characterId != 0 && lot > 0) return ExecuteSelect(sql, characterId, characterId, lot, extra...);
		if (characterId != 0) return ExecuteSelect(sql, characterId, characterId, extra...);
		if (lot > 0) return ExecuteSelect(sql, lot, extra...);
		return ExecuteSelect(sql, extra...);
	};

	auto [_, countResult] = run(countQuery);
	const auto total = countResult.eof() ? 0 : countResult.getIntField("count");
	auto [__, result] = run(query, length, start);
	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		data.push_back(TransferRow(result));
		result.nextRow();
	}
	return { {"draw", 0}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} };
}

nlohmann::json SQLiteDatabase::GetTransfersForItems(const std::vector<LWOOBJID>& itemIds) {
	nlohmann::json rows = nlohmann::json::array();
	if (itemIds.empty()) return rows;
	// Ids are numbers, so listing them inline is safe
	std::string list;
	for (const auto id : itemIds) list += (list.empty() ? "" : ",") + std::to_string(id);
	auto [_, result] = ExecuteSelect(std::string(TRANSFER_SELECT) + " WHERE t.item_id IN (" + list + ") OR t.new_item_id IN (" + list + ") ORDER BY t.id;");
	while (!result.eof()) {
		rows.push_back(TransferRow(result));
		result.nextRow();
	}
	return rows;
}

void SQLiteDatabase::ForEachMailAttachment(const std::function<void(const MailAttachment&)>& visit) {
	auto [_, result] = ExecuteSelect("SELECT id, receiver_id, attachment_id, attachment_lot, attachment_count FROM mail WHERE attachment_lot > 0 AND attachment_count > 0 AND deleted_at = 0;");
	while (!result.eof()) {
		visit({ static_cast<uint64_t>(result.getInt64Field("id")), result.getInt64Field("receiver_id"), result.getInt64Field("attachment_id"),
			result.getIntField("attachment_lot"), static_cast<uint32_t>(result.getIntField("attachment_count")) });
		result.nextRow();
	}
}

namespace {
	// " AND clone_id = n" for one clone, nothing for every instance
	std::string CloneSql(std::optional<uint32_t> clone) {
		return clone ? " AND clone_id = " + std::to_string(*clone) : "";
	}

	std::string IdList(const std::vector<uint32_t>& ids) {
		std::string list;
		for (const auto id : ids) list += (list.empty() ? "" : ",") + std::to_string(id);
		return list;
	}
}

nlohmann::json SQLiteDatabase::GetMapZones(eMapEvent kind, uint32_t fromDay, uint32_t toDay) {
	auto [_, result] = ExecuteSelect("SELECT zone, clone_id, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE kind = ? AND day BETWEEN ? AND ? GROUP BY zone, clone_id ORDER BY events DESC, zone, clone_id;", static_cast<uint32_t>(kind), fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"zone", result.getIntField("zone")}, {"clone", static_cast<uint32_t>(result.getInt64Field("clone_id"))}, {"events", result.getInt64Field("events")}, {"quantity", result.getInt64Field("quantity")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetMapZonesAllKinds(uint32_t fromDay, uint32_t toDay) {
	auto [_, result] = ExecuteSelect("SELECT kind, zone, clone_id, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE day BETWEEN ? AND ? GROUP BY kind, zone, clone_id ORDER BY kind, events DESC, zone, clone_id;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"kind", result.getIntField("kind")}, {"zone", result.getIntField("zone")}, {"clone", static_cast<uint32_t>(result.getInt64Field("clone_id"))},
			{"events", result.getInt64Field("events")}, {"quantity", result.getInt64Field("quantity")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetMapLots(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT lot, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE zone = ? AND kind = ? AND day BETWEEN ? AND ?" + CloneSql(clone) + " GROUP BY lot ORDER BY events DESC, lot LIMIT ?;", zone, static_cast<uint32_t>(kind), fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"lot", result.getIntField("lot")}, {"events", result.getInt64Field("events")}, {"quantity", result.getInt64Field("quantity")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetMapCells(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, LOT lot) {
	const std::string query = std::string("SELECT cell_x, cell_z, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE zone = ? AND kind = ? AND day BETWEEN ? AND ?") + CloneSql(clone) + (lot > 0 ? " AND lot = ?" : "") + " GROUP BY cell_x, cell_z;";
	auto [_, result] = lot > 0 ? ExecuteSelect(query, zone, static_cast<uint32_t>(kind), fromDay, toDay, lot) : ExecuteSelect(query, zone, static_cast<uint32_t>(kind), fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"x", result.getIntField("cell_x")}, {"z", result.getIntField("cell_z")}, {"events", result.getInt64Field("events")}, {"quantity", result.getInt64Field("quantity")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetMapEventsPerDay(uint32_t fromDay, uint32_t toDay, const PlaceFilter& place) {
	auto [_, result] = ExecuteSelect("SELECT day, kind, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE day BETWEEN ? AND ?" + place.Sql() + " GROUP BY day, kind ORDER BY day, kind;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"kind", result.getIntField("kind")}, {"events", result.getInt64Field("events")}, {"quantity", result.getInt64Field("quantity")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetMapEventsByLot(eMapEvent kind, uint32_t fromDay, uint32_t toDay, const PlaceFilter& place, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT day, zone, clone_id, lot, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE kind = ? AND day BETWEEN ? AND ?" + place.Sql() + " GROUP BY day, zone, clone_id, lot ORDER BY day, zone, clone_id, lot LIMIT ?;",
		static_cast<uint32_t>(kind), fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"zone", result.getIntField("zone")}, {"clone", static_cast<uint32_t>(result.getInt64Field("clone_id"))},
			{"lot", result.getIntField("lot")}, {"events", result.getInt64Field("events")}, {"quantity", result.getInt64Field("quantity")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetPlayerStatsPerDay(uint32_t fromDay, uint32_t toDay, bool excludeStaff, const PlaceFilter& place) {
	auto [_, result] = ExecuteSelect(std::string("SELECT day, stat, SUM(amount) AS amount FROM player_stats_daily WHERE day BETWEEN ? AND ?") +
		(excludeStaff ? " AND gm = 0" : "") + place.Sql() + " GROUP BY day, stat ORDER BY day, stat;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"day", result.getIntField("day")}, {"stat", result.getIntField("stat")}, {"amount", result.getInt64Field("amount")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetPlayerStatsPerZone(uint32_t fromDay, uint32_t toDay, bool excludeStaff) {
	auto [_, result] = ExecuteSelect(std::string("SELECT zone, clone_id, stat, SUM(amount) AS amount FROM player_stats_daily WHERE day BETWEEN ? AND ?") +
		(excludeStaff ? " AND gm = 0" : "") + " GROUP BY zone, clone_id, stat ORDER BY amount DESC, zone, clone_id, stat;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	for (; !result.eof(); result.nextRow()) {
		rows.push_back({ {"zone", result.getIntField("zone")}, {"clone", static_cast<uint32_t>(result.getInt64Field("clone_id"))}, {"stat", result.getIntField("stat")}, {"amount", result.getInt64Field("amount")} });
	}
	return rows;
}

nlohmann::json SQLiteDatabase::GetCloneOwners(const std::vector<uint32_t>& clones) {
	nlohmann::json owners = nlohmann::json::array();
	nlohmann::json properties = nlohmann::json::array();
	if (!clones.empty()) {
		const auto list = IdList(clones);
		{
			auto [_, result] = ExecuteSelect("SELECT prop_clone_id, id, name FROM charinfo WHERE prop_clone_id IN (" + list + ") ORDER BY prop_clone_id;");
			for (; !result.eof(); result.nextRow()) {
				owners.push_back({ {"clone", static_cast<uint32_t>(result.getInt64Field("prop_clone_id"))}, {"character_id", std::to_string(result.getInt64Field("id"))},
					{"name", std::string(result.getStringField("name"))} });
			}
		}
		auto [_, props] = ExecuteSelect("SELECT clone_id, zone_id, id, name, owner_id FROM properties WHERE clone_id IN (" + list + ") ORDER BY clone_id, zone_id;");
		for (; !props.eof(); props.nextRow()) {
			properties.push_back({ {"clone", static_cast<uint32_t>(props.getInt64Field("clone_id"))}, {"zone", props.getIntField("zone_id")},
				{"id", std::to_string(props.getInt64Field("id"))}, {"name", std::string(props.getStringField("name"))}, {"owner_id", std::to_string(props.getInt64Field("owner_id"))} });
		}
	}
	return { {"owners", owners}, {"properties", properties} };
}
