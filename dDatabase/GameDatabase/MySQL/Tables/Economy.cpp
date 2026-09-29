#include "MySQLDatabase.h"

namespace {
	// Staff (GM 3+) are excluded from player reports, matching NexusDashboard. Deleted characters still count.
	// The few staff characters are looked up once; joining every ledger row to its character and account took several
	// times as long on a large ledger.
	constexpr const char* STAFF_CHARACTERS = "SELECT c.id FROM charinfo c JOIN accounts a ON a.id = c.account_id WHERE a.gm_level >= 3";
	const std::string STAFF_FILTER = std::string(" WHERE f.character_id NOT IN (") + STAFF_CHARACTERS + ") AND ";
	constexpr const char* NO_FILTER = " WHERE ";
}

void MySQLDatabase::RecordEconomy(const std::vector<CurrencyFlow>& currency, const std::vector<UScoreFlow>& uscore,
	const std::vector<ItemFlow>& items, const std::vector<ItemTransfer>& transfers, const std::vector<MapEvent>& mapEvents,
	const std::vector<PlayerStat>& stats) {
	if (currency.empty() && uscore.empty() && items.empty() && transfers.empty() && mapEvents.empty() && stats.empty()) return;

	DatabaseTransaction transaction(*this);
	for (const auto& flow : currency) {
		ExecuteInsert(
			"INSERT INTO economy_currency_daily (day, character_id, source, gained, spent) VALUES (?, ?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE gained = gained + VALUES(gained), spent = spent + VALUES(spent);",
			flow.day, flow.characterId, flow.source, flow.gained, flow.spent);
	}
	for (const auto& flow : uscore) {
		ExecuteInsert(
			"INSERT INTO economy_uscore_daily (day, character_id, source, gained, lost) VALUES (?, ?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE gained = gained + VALUES(gained), lost = lost + VALUES(lost);",
			flow.day, flow.characterId, flow.source, flow.gained, flow.lost);
	}
	for (const auto& flow : items) {
		ExecuteInsert(
			"INSERT INTO economy_items_daily (day, lot, source, gm, created, destroyed) VALUES (?, ?, ?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE created = created + VALUES(created), destroyed = destroyed + VALUES(destroyed);",
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
			"ON DUPLICATE KEY UPDATE events = events + VALUES(events), quantity = quantity + VALUES(quantity);",
			event.day, event.zone, event.clone, static_cast<uint32_t>(event.kind), event.lot, event.cellX, event.cellZ, event.events, event.quantity);
	}
	for (const auto& stat : stats) {
		ExecuteInsert(
			"INSERT INTO player_stats_daily (day, zone, clone_id, stat, gm, amount) VALUES (?, ?, ?, ?, ?, ?) "
			"ON DUPLICATE KEY UPDATE amount = amount + VALUES(amount);",
			stat.day, stat.zone, stat.clone, stat.stat, stat.gm ? 1 : 0, stat.amount);
	}
	transaction.Commit();
}

nlohmann::json MySQLDatabase::GetCurrencyFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) {
	auto result = ExecuteSelect(std::string("SELECT f.day, f.source, SUM(f.gained) AS gained, SUM(f.spent) AS spent FROM economy_currency_daily f") +
		(excludeStaff ? STAFF_FILTER : NO_FILTER) + "f.day BETWEEN ? AND ? GROUP BY f.day, f.source ORDER BY f.day;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"day", result->getInt("day")}, {"source", result->getInt("source")},
			{"gained", result->getInt64("gained")}, {"spent", result->getInt64("spent")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetUScoreFlows(uint32_t fromDay, uint32_t toDay, bool excludeStaff) {
	auto result = ExecuteSelect(std::string("SELECT f.day, f.source, SUM(f.gained) AS gained, SUM(f.lost) AS lost FROM economy_uscore_daily f") +
		(excludeStaff ? STAFF_FILTER : NO_FILTER) + "f.day BETWEEN ? AND ? GROUP BY f.day, f.source ORDER BY f.day;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"day", result->getInt("day")}, {"source", result->getInt("source")},
			{"gained", result->getInt64("gained")}, {"lost", result->getInt64("lost")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetItemFlows(uint32_t fromDay, uint32_t toDay, LOT lot, bool excludeStaff) {
	const std::string query = std::string("SELECT day, source, SUM(created) AS created, SUM(destroyed) AS destroyed FROM economy_items_daily "
		"WHERE day BETWEEN ? AND ?") + (excludeStaff ? " AND gm = 0" : "") + (lot > 0 ? " AND lot = ?" : "") + " GROUP BY day, source ORDER BY day;";
	auto result = lot > 0 ? ExecuteSelect(query, fromDay, toDay, lot) : ExecuteSelect(query, fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"day", result->getInt("day")}, {"source", result->getInt("source")},
			{"created", result->getInt64("created")}, {"destroyed", result->getInt64("destroyed")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetTopEarners(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) {
	// Trades and mail move coins between players rather than creating them, so they are left out of "income"
	// Totals per character first, names for the top ones only
	auto result = ExecuteSelect(std::string(
		"SELECT t.character_id, COALESCE(c.name, '') AS name, t.gained, t.spent FROM (SELECT f.character_id, SUM(f.gained) AS gained, "
		"SUM(f.spent) AS spent FROM economy_currency_daily f WHERE f.day BETWEEN ? AND ? AND f.source NOT IN (3, 6)") +
		(excludeStaff ? std::string(" AND f.character_id NOT IN (") + STAFF_CHARACTERS + ")" : std::string()) +
		" GROUP BY f.character_id ORDER BY gained DESC LIMIT ?) t LEFT JOIN charinfo c ON c.id = t.character_id ORDER BY t.gained DESC;", fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"character_id", std::to_string(result->getInt64("character_id"))}, {"name", std::string(result->getString("name").c_str())},
			{"gained", result->getInt64("gained")}, {"spent", result->getInt64("spent")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetTopItems(uint32_t fromDay, uint32_t toDay, uint32_t limit, bool excludeStaff) {
	auto result = ExecuteSelect(std::string(
		"SELECT lot, SUM(created) AS created, SUM(destroyed) AS destroyed FROM economy_items_daily WHERE day BETWEEN ? AND ?") +
		(excludeStaff ? " AND gm = 0" : "") + " GROUP BY lot ORDER BY created DESC LIMIT ?;", fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"lot", result->getInt("lot")}, {"created", result->getInt64("created")}, {"destroyed", result->getInt64("destroyed")} });
	}
	return rows;
}

namespace {
	nlohmann::json TransferRow(PreparedStmtResultSet& result) {
		return {
			{"id", result->getInt64("id")},
			{"time", result->getInt64("time")},
			{"method", result->getInt("method")},
			{"item_id", std::to_string(result->getInt64("item_id"))},
			{"new_item_id", std::to_string(result->getInt64("new_item_id"))},
			{"lot", result->getInt("lot")},
			{"count", result->getInt("count")},
			{"coins", result->getInt64("coins")},
			{"from_character", std::to_string(result->getInt64("from_character"))},
			{"from_name", result->isNull("from_name") ? "" : std::string(result->getString("from_name").c_str())},
			{"to_character", std::to_string(result->getInt64("to_character"))},
			{"to_name", result->isNull("to_name") ? "" : std::string(result->getString("to_name").c_str())},
			{"zone", result->getInt("zone")},
			{"merged", result->isNull("merged") ? nlohmann::json() : nlohmann::json(result->getInt("merged") != 0)}
		};
	}

	constexpr const char* TRANSFER_SELECT =
		"SELECT t.*, cf.name AS from_name, ct.name AS to_name FROM economy_transfers t "
		"LEFT JOIN charinfo cf ON cf.id = t.from_character LEFT JOIN charinfo ct ON ct.id = t.to_character";
}

nlohmann::json MySQLDatabase::GetTransfers(uint32_t start, uint32_t length, LWOOBJID characterId, LOT lot) {
	std::string where = " WHERE t.method <> " + std::to_string(static_cast<uint32_t>(eTransferMethod::INVENTORY_MOVE));
	if (characterId != 0) where += " AND (t.from_character = ? OR t.to_character = ?)";
	if (lot > 0) where += " AND t.lot = ?";

	const auto countQuery = "SELECT COUNT(*) AS count FROM economy_transfers t" + where + ";";
	const auto query = std::string(TRANSFER_SELECT) + where + " ORDER BY t.id DESC LIMIT ?, ?;";
	auto run = [&](const std::string& sql, auto... extra) {
		if (characterId != 0 && lot > 0) return ExecuteSelect(sql, characterId, characterId, lot, extra...);
		if (characterId != 0) return ExecuteSelect(sql, characterId, characterId, extra...);
		if (lot > 0) return ExecuteSelect(sql, lot, extra...);
		return ExecuteSelect(sql, extra...);
	};

	auto countResult = run(countQuery);
	const auto total = countResult->next() ? countResult->getUInt("count") : 0;
	auto result = run(query, start, length);
	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		data.push_back(TransferRow(result));
	}
	return { {"draw", 0}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} };
}

nlohmann::json MySQLDatabase::GetTransfersForItems(const std::vector<LWOOBJID>& itemIds) {
	nlohmann::json rows = nlohmann::json::array();
	if (itemIds.empty()) return rows;
	// Ids are numbers, so listing them inline is safe
	std::string list;
	for (const auto id : itemIds) list += (list.empty() ? "" : ",") + std::to_string(id);
	auto result = ExecuteSelect(std::string(TRANSFER_SELECT) + " WHERE t.item_id IN (" + list + ") OR t.new_item_id IN (" + list + ") ORDER BY t.id;");
	while (result->next()) {
		rows.push_back(TransferRow(result));
	}
	return rows;
}

void MySQLDatabase::ForEachMailAttachment(const std::function<void(const MailAttachment&)>& visit) {
	auto result = ExecuteSelect("SELECT id, receiver_id, attachment_id, attachment_lot, attachment_count FROM mail WHERE attachment_lot > 0 AND attachment_count > 0 AND deleted_at = 0;");
	while (result->next()) {
		visit({ result->getUInt64("id"), result->getInt64("receiver_id"), result->getInt64("attachment_id"),
			result->getInt("attachment_lot"), static_cast<uint32_t>(result->getInt("attachment_count")) });
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

nlohmann::json MySQLDatabase::GetMapZones(eMapEvent kind, uint32_t fromDay, uint32_t toDay) {
	auto result = ExecuteSelect("SELECT zone, clone_id, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE kind = ? AND day BETWEEN ? AND ? GROUP BY zone, clone_id ORDER BY events DESC, zone, clone_id;", static_cast<uint32_t>(kind), fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"zone", result->getInt("zone")}, {"clone", result->getUInt("clone_id")}, {"events", result->getInt64("events")}, {"quantity", result->getInt64("quantity")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetMapZonesAllKinds(uint32_t fromDay, uint32_t toDay) {
	auto result = ExecuteSelect("SELECT kind, zone, clone_id, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE day BETWEEN ? AND ? GROUP BY kind, zone, clone_id ORDER BY kind, events DESC, zone, clone_id;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"kind", result->getInt("kind")}, {"zone", result->getInt("zone")}, {"clone", result->getUInt("clone_id")},
			{"events", result->getInt64("events")}, {"quantity", result->getInt64("quantity")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetMapLots(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, uint32_t limit) {
	auto result = ExecuteSelect("SELECT lot, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE zone = ? AND kind = ? AND day BETWEEN ? AND ?" + CloneSql(clone) + " GROUP BY lot ORDER BY events DESC, lot LIMIT ?;", zone, static_cast<uint32_t>(kind), fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"lot", result->getInt("lot")}, {"events", result->getInt64("events")}, {"quantity", result->getInt64("quantity")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetMapCells(uint32_t zone, std::optional<uint32_t> clone, eMapEvent kind, uint32_t fromDay, uint32_t toDay, LOT lot) {
	const std::string query = std::string("SELECT cell_x, cell_z, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE zone = ? AND kind = ? AND day BETWEEN ? AND ?") + CloneSql(clone) + (lot > 0 ? " AND lot = ?" : "") + " GROUP BY cell_x, cell_z;";
	auto result = lot > 0 ? ExecuteSelect(query, zone, static_cast<uint32_t>(kind), fromDay, toDay, lot) : ExecuteSelect(query, zone, static_cast<uint32_t>(kind), fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"x", result->getInt("cell_x")}, {"z", result->getInt("cell_z")}, {"events", result->getInt64("events")}, {"quantity", result->getInt64("quantity")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetMapEventsPerDay(uint32_t fromDay, uint32_t toDay, const PlaceFilter& place) {
	auto result = ExecuteSelect("SELECT day, kind, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE day BETWEEN ? AND ?" + place.Sql() + " GROUP BY day, kind ORDER BY day, kind;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"day", result->getInt("day")}, {"kind", result->getInt("kind")}, {"events", result->getInt64("events")}, {"quantity", result->getInt64("quantity")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetMapEventsByLot(eMapEvent kind, uint32_t fromDay, uint32_t toDay, const PlaceFilter& place, uint32_t limit) {
	auto result = ExecuteSelect("SELECT day, zone, clone_id, lot, SUM(events) AS events, SUM(quantity) AS quantity FROM map_events_daily "
		"WHERE kind = ? AND day BETWEEN ? AND ?" + place.Sql() + " GROUP BY day, zone, clone_id, lot ORDER BY day, zone, clone_id, lot LIMIT ?;",
		static_cast<uint32_t>(kind), fromDay, toDay, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"day", result->getInt("day")}, {"zone", result->getInt("zone")}, {"clone", result->getUInt("clone_id")}, {"lot", result->getInt("lot")},
			{"events", result->getInt64("events")}, {"quantity", result->getInt64("quantity")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetPlayerStatsPerDay(uint32_t fromDay, uint32_t toDay, bool excludeStaff, const PlaceFilter& place) {
	auto result = ExecuteSelect(std::string("SELECT day, stat, SUM(amount) AS amount FROM player_stats_daily WHERE day BETWEEN ? AND ?") +
		(excludeStaff ? " AND gm = 0" : "") + place.Sql() + " GROUP BY day, stat ORDER BY day, stat;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"day", result->getInt("day")}, {"stat", result->getInt("stat")}, {"amount", result->getInt64("amount")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetPlayerStatsPerZone(uint32_t fromDay, uint32_t toDay, bool excludeStaff) {
	auto result = ExecuteSelect(std::string("SELECT zone, clone_id, stat, SUM(amount) AS amount FROM player_stats_daily WHERE day BETWEEN ? AND ?") +
		(excludeStaff ? " AND gm = 0" : "") + " GROUP BY zone, clone_id, stat ORDER BY amount DESC, zone, clone_id, stat;", fromDay, toDay);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({ {"zone", result->getInt("zone")}, {"clone", result->getUInt("clone_id")}, {"stat", result->getInt("stat")}, {"amount", result->getInt64("amount")} });
	}
	return rows;
}

nlohmann::json MySQLDatabase::GetCloneOwners(const std::vector<uint32_t>& clones) {
	nlohmann::json owners = nlohmann::json::array();
	nlohmann::json properties = nlohmann::json::array();
	if (!clones.empty()) {
		const auto list = IdList(clones);
		auto result = ExecuteSelect("SELECT prop_clone_id, id, name FROM charinfo WHERE prop_clone_id IN (" + list + ") ORDER BY prop_clone_id;");
		while (result->next()) {
			owners.push_back({ {"clone", result->getUInt("prop_clone_id")}, {"character_id", std::to_string(result->getInt64("id"))}, {"name", std::string(result->getString("name").c_str())} });
		}
		auto props = ExecuteSelect("SELECT clone_id, zone_id, id, name, owner_id FROM properties WHERE clone_id IN (" + list + ") ORDER BY clone_id, zone_id;");
		while (props->next()) {
			properties.push_back({ {"clone", props->getUInt("clone_id")}, {"zone", props->getInt("zone_id")}, {"id", std::to_string(props->getInt64("id"))},
				{"name", std::string(props->getString("name").c_str())}, {"owner_id", std::to_string(props->getInt64("owner_id"))} });
		}
	}
	return { {"owners", owners}, {"properties", properties} };
}
