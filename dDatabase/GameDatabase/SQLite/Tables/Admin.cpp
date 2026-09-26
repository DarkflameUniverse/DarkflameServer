#include "SQLiteDatabase.h"

#include <ctime>

namespace {
	IDashboardAdmin::Webhook WebhookRow(CppSQLite3Query& result) {
		IDashboardAdmin::Webhook webhook;
		webhook.id = static_cast<uint32_t>(result.getIntField("id"));
		webhook.name = result.getStringField("name");
		webhook.url = result.getStringField("url");
		webhook.format = result.getStringField("format");
		webhook.events = result.getStringField("events");
		webhook.secret = result.getStringField("secret");
		webhook.enabled = result.getIntField("enabled") != 0;
		webhook.createdAt = result.getInt64Field("created_at");
		webhook.lastSentAt = result.getInt64Field("last_sent_at");
		webhook.lastStatus = result.getIntField("last_status");
		webhook.lastError = result.fieldIsNull("last_error") ? "" : result.getStringField("last_error");
		return webhook;
	}

	// A ledger table: its key columns (besides day) and the value columns that are summed
	struct LedgerTable {
		const char* name;
		const char* keys;
		const char* conflict; // the primary key, for ON CONFLICT
		std::vector<const char*> values;
	};

	const std::vector<LedgerTable> DAILY_TABLES{
		{ "economy_currency_daily", "character_id, source", "day, character_id, source", { "gained", "spent" } },
		{ "economy_uscore_daily", "character_id, source", "day, character_id, source", { "gained", "lost" } },
		{ "economy_items_daily", "lot, source, gm", "day, lot, source, gm", { "created", "destroyed" } },
		{ "player_stats_daily", "zone, clone_id, stat, gm", "day, zone, clone_id, stat, gm", { "amount" } },
	};
	const LedgerTable MAP_TABLE{ "map_events_daily", "zone, clone_id, kind, lot, cell_x, cell_z", "zone, kind, day, clone_id, lot, cell_x, cell_z", { "events", "quantity" } };
}

std::vector<IDashboardAdmin::Webhook> SQLiteDatabase::GetWebhooks() {
	auto [_, result] = ExecuteSelect("SELECT * FROM dashboard_webhooks ORDER BY id;");
	std::vector<Webhook> webhooks;
	while (!result.eof()) {
		webhooks.push_back(WebhookRow(result));
		result.nextRow();
	}
	return webhooks;
}

std::optional<IDashboardAdmin::Webhook> SQLiteDatabase::GetWebhook(uint32_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM dashboard_webhooks WHERE id = ?;", id);
	if (result.eof()) return std::nullopt;
	return WebhookRow(result);
}

void SQLiteDatabase::InsertWebhook(const Webhook& webhook) {
	ExecuteInsert("INSERT INTO dashboard_webhooks (name, url, format, events, secret, enabled, created_at) VALUES (?, ?, ?, ?, ?, ?, ?);",
		webhook.name, webhook.url, webhook.format, webhook.events, webhook.secret, webhook.enabled, static_cast<int64_t>(std::time(nullptr)));
}

void SQLiteDatabase::UpdateWebhook(const Webhook& webhook) {
	ExecuteUpdate("UPDATE dashboard_webhooks SET name = ?, url = ?, format = ?, events = ?, secret = ?, enabled = ? WHERE id = ?;",
		webhook.name, webhook.url, webhook.format, webhook.events, webhook.secret, webhook.enabled, webhook.id);
}

void SQLiteDatabase::DeleteWebhook(uint32_t id) {
	ExecuteDelete("DELETE FROM dashboard_webhooks WHERE id = ?;", id);
}

void SQLiteDatabase::RecordWebhookResult(uint32_t id, int64_t time, int32_t status, const std::string& error) {
	ExecuteUpdate("UPDATE dashboard_webhooks SET last_sent_at = ?, last_status = ?, last_error = ? WHERE id = ?;", time, status, error, id);
}

std::optional<std::string> SQLiteDatabase::GetDashboardState(const std::string& name) {
	auto [_, result] = ExecuteSelect("SELECT value FROM dashboard_state WHERE name = ?;", name);
	if (result.eof()) return std::nullopt;
	return std::string(result.getStringField("value"));
}

void SQLiteDatabase::SetDashboardState(const std::string& name, const std::string& value) {
	ExecuteInsert("INSERT INTO dashboard_state (name, value) VALUES (?, ?) ON CONFLICT(name) DO UPDATE SET value = excluded.value;", name, value);
}

void SQLiteDatabase::DeleteDashboardState(const std::string& name) {
	ExecuteDelete("DELETE FROM dashboard_state WHERE name = ?;", name);
}

std::string SQLiteDatabase::GetDashboardPreferences(uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT prefs FROM dashboard_preferences WHERE account_id = ?;", accountId);
	if (result.eof()) return "{}";
	return std::string(result.getStringField("prefs"));
}

void SQLiteDatabase::SetDashboardPreferences(uint32_t accountId, const std::string& prefs) {
	ExecuteInsert("INSERT INTO dashboard_preferences (account_id, prefs) VALUES (?, ?) ON CONFLICT(account_id) DO UPDATE SET prefs = excluded.prefs;", accountId, prefs);
}

bool SQLiteDatabase::InsertEconomyFlag(const EconomyFlag& flag) {
	return ExecuteUpdate(
		"INSERT OR IGNORE INTO economy_flags (created_at, day, kind, character_id, lot, item_id, value, baseline, details) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
		static_cast<int64_t>(std::time(nullptr)), flag.day, static_cast<uint32_t>(flag.kind), flag.characterId, flag.lot, flag.itemId,
		flag.value, flag.baseline, flag.details) > 0;
}

nlohmann::json SQLiteDatabase::GetEconomyFlagsTable(uint32_t start, uint32_t length, int32_t status) {
	const std::string where = status >= 0 ? " WHERE f.status = ?" : "";
	const auto count = [&]() {
		auto [_, result] = status >= 0 ? ExecuteSelect("SELECT COUNT(*) AS count FROM economy_flags f" + where + ";", status)
			: ExecuteSelect("SELECT COUNT(*) AS count FROM economy_flags f;");
		return result.eof() ? 0 : result.getIntField("count");
	}();
	const std::string query = "SELECT f.*, c.name AS character_name, a.name AS reviewer_name FROM economy_flags f "
		"LEFT JOIN charinfo c ON c.id = f.character_id LEFT JOIN accounts a ON a.id = f.reviewed_by" + where + " ORDER BY f.id DESC LIMIT ? OFFSET ?;";
	auto [_, result] = status >= 0 ? ExecuteSelect(query, status, length, start) : ExecuteSelect(query, length, start);
	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		data.push_back({
			{"id", result.getInt64Field("id")},
			{"created_at", result.getInt64Field("created_at")},
			{"day", result.getIntField("day")},
			{"kind", result.getIntField("kind")},
			{"character_id", std::to_string(result.getInt64Field("character_id"))},
			{"character_name", result.fieldIsNull("character_name") ? "" : result.getStringField("character_name")},
			{"lot", result.getIntField("lot")},
			{"item_id", std::to_string(result.getInt64Field("item_id"))},
			{"value", result.getInt64Field("value")},
			{"baseline", result.getInt64Field("baseline")},
			{"details", result.fieldIsNull("details") ? "" : result.getStringField("details")},
			{"status", result.getIntField("status")},
			{"reviewer_name", result.fieldIsNull("reviewer_name") ? "" : result.getStringField("reviewer_name")},
			{"reviewed_at", result.getInt64Field("reviewed_at")},
			{"note", result.fieldIsNull("note") ? "" : result.getStringField("note")}
		});
		result.nextRow();
	}
	return { {"draw", 0}, {"recordsTotal", count}, {"recordsFiltered", count}, {"data", data} };
}

void SQLiteDatabase::ReviewEconomyFlag(uint64_t id, eFlagStatus status, uint32_t reviewerAccountId, const std::string& note) {
	ExecuteUpdate("UPDATE economy_flags SET status = ?, reviewed_by = ?, reviewed_at = ?, note = ? WHERE id = ?;",
		static_cast<uint32_t>(status), reviewerAccountId, static_cast<int64_t>(std::time(nullptr)), note, id);
}

uint32_t SQLiteDatabase::GetOpenEconomyFlagCount() {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM economy_flags WHERE status = 0;");
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
}

std::vector<std::pair<LWOOBJID, int64_t>> SQLiteDatabase::GetDailyIncome(uint32_t day) {
	auto [_, result] = ExecuteSelect(
		"SELECT f.character_id, SUM(f.gained) AS gained FROM economy_currency_daily f "
		"LEFT JOIN charinfo c ON c.id = f.character_id LEFT JOIN accounts a ON a.id = c.account_id "
		"WHERE f.day = ? AND f.source NOT IN (3, 6) AND COALESCE(a.gm_level, 0) < 3 GROUP BY f.character_id HAVING SUM(f.gained) > 0;", day);
	std::vector<std::pair<LWOOBJID, int64_t>> rows;
	while (!result.eof()) {
		rows.emplace_back(result.getInt64Field("character_id"), result.getInt64Field("gained"));
		result.nextRow();
	}
	return rows;
}

std::map<LOT, int64_t> SQLiteDatabase::GetItemCreationTotals(uint32_t fromDay, uint32_t toDay) {
	auto [_, result] = ExecuteSelect(
		"SELECT lot, SUM(created) AS created FROM economy_items_daily WHERE day BETWEEN ? AND ? AND gm = 0 GROUP BY lot;", fromDay, toDay);
	std::map<LOT, int64_t> totals;
	while (!result.eof()) {
		totals[result.getIntField("lot")] = result.getInt64Field("created");
		result.nextRow();
	}
	return totals;
}

uint32_t SQLiteDatabase::CompactEconomy(uint32_t cutoffDay, uint32_t mapCutoffDay) {
	uint32_t merged = 0;
	const auto compact = [&](const LedgerTable& table, uint32_t cutoff) {
		std::vector<uint32_t> days;
		{
			auto [_, result] = ExecuteSelect(std::string("SELECT DISTINCT day FROM ") + table.name + " WHERE day < ?;", cutoff);
			while (!result.eof()) {
				days.push_back(static_cast<uint32_t>(result.getIntField("day")));
				result.nextRow();
			}
		}
		std::string values, sums;
		for (const auto* value : table.values) {
			values += std::string(", ") + value;
			sums += std::string(sums.empty() ? "" : ", ") + value + " = " + value + " + excluded." + value;
		}
		for (const auto day : days) {
			const auto month = MonthStartDay(day);
			if (month == day) continue;
			// One short transaction per day, so the world servers' saves never wait long for the write lock
			DatabaseTransaction transaction(*this);
			ExecuteInsert(std::string("INSERT INTO ") + table.name + " (day, " + table.keys + values + ") SELECT ?, " + table.keys + values +
				" FROM " + table.name + " WHERE day = ? ON CONFLICT(" + table.conflict + ") DO UPDATE SET " + sums + ";", month, day);
			const auto deleted = static_cast<uint32_t>(ExecuteUpdate(std::string("DELETE FROM ") + table.name + " WHERE day = ?;", day));
			transaction.Commit();
			merged += deleted;
		}
	};
	for (const auto& table : DAILY_TABLES) compact(table, cutoffDay);
	compact(MAP_TABLE, mapCutoffDay);
	return merged;
}

uint32_t SQLiteDatabase::PruneTransfers(int64_t beforeTime) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM economy_transfers WHERE time < ?;", beforeTime));
}

IDashboardAdmin::Totp SQLiteDatabase::GetTotp(uint32_t accountId) {
	Totp totp;
	auto [_, result] = ExecuteSelect("SELECT totp_secret, totp_enabled_at, totp_last_step FROM accounts WHERE id = ?;", accountId);
	if (result.eof()) return totp;
	totp.encryptedSecret = result.fieldIsNull("totp_secret") ? "" : result.getStringField("totp_secret");
	totp.enabledAt = result.getInt64Field("totp_enabled_at");
	totp.lastStep = result.getInt64Field("totp_last_step");
	return totp;
}

void SQLiteDatabase::SetTotp(uint32_t accountId, const std::string& encryptedSecret, int64_t enabledAt) {
	ExecuteUpdate("UPDATE accounts SET totp_secret = ?, totp_enabled_at = ?, totp_last_step = 0 WHERE id = ?;", encryptedSecret, enabledAt, accountId);
}

bool SQLiteDatabase::UseTotpStep(uint32_t accountId, int64_t step) {
	return ExecuteUpdate("UPDATE accounts SET totp_last_step = ? WHERE id = ? AND totp_last_step < ?;", step, accountId, step) > 0;
}

void SQLiteDatabase::ReplaceRecoveryCodes(uint32_t accountId, const std::vector<std::string>& codeHashes) {
	ExecuteDelete("DELETE FROM account_recovery_codes WHERE account_id = ?;", accountId);
	for (const auto& hash : codeHashes) ExecuteInsert("INSERT INTO account_recovery_codes (account_id, code_hash) VALUES (?, ?);", accountId, hash);
}

bool SQLiteDatabase::UseRecoveryCode(uint32_t accountId, const std::string& codeHash) {
	return ExecuteUpdate("UPDATE account_recovery_codes SET used_at = ? WHERE account_id = ? AND code_hash = ? AND used_at = 0;",
		static_cast<int64_t>(std::time(nullptr)), accountId, codeHash) > 0;
}

uint32_t SQLiteDatabase::GetRecoveryCodesLeft(uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM account_recovery_codes WHERE account_id = ? AND used_at = 0;", accountId);
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
}

nlohmann::json SQLiteDatabase::GetBugReportsAfter(uint32_t afterId, uint32_t limit) {
	auto [_, result] = ExecuteSelect(
		"SELECT b.id, b.body, b.client_version, b.other_player_id, b.selection, b.reporter_id, c.name AS reporter_name FROM bug_reports b "
		"LEFT JOIN charinfo c ON c.id = b.reporter_id WHERE b.id > ? ORDER BY b.id LIMIT ?;", afterId, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (!result.eof()) {
		rows.push_back({
			{"id", result.getIntField("id")}, {"body", result.getStringField("body")}, {"client_version", result.getStringField("client_version")},
			{"other_player_id", result.getStringField("other_player_id")}, {"selection", result.getStringField("selection")},
			{"reporter_id", std::to_string(result.getInt64Field("reporter_id"))},
			{"reporter_name", result.fieldIsNull("reporter_name") ? "" : result.getStringField("reporter_name")}
		});
		result.nextRow();
	}
	return rows;
}

uint32_t SQLiteDatabase::GetMaxBugReportId() {
	auto [_, result] = ExecuteSelect("SELECT COALESCE(MAX(id), 0) AS id FROM bug_reports;");
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("id"));
}

nlohmann::json SQLiteDatabase::GetTransfersForCharacters(const std::vector<LWOOBJID>& characterIds, uint32_t start, uint32_t length) {
	nlohmann::json empty{ {"draw", 0}, {"recordsTotal", 0}, {"recordsFiltered", 0}, {"data", nlohmann::json::array()} };
	if (characterIds.empty()) return empty;
	// Ids are numbers from the database, so listing them inline is safe
	std::string list;
	for (const auto id : characterIds) list += (list.empty() ? "" : ",") + std::to_string(id);
	const std::string where = " WHERE (t.from_character IN (" + list + ") OR t.to_character IN (" + list + ")) AND t.method <> " +
		std::to_string(static_cast<uint32_t>(IEconomyLedger::eTransferMethod::INVENTORY_MOVE));

	auto [_, countResult] = ExecuteSelect("SELECT COUNT(*) AS count FROM economy_transfers t" + where + ";");
	const auto total = countResult.eof() ? 0 : countResult.getIntField("count");
	auto [__, result] = ExecuteSelect(
		"SELECT t.*, cf.name AS from_name, ct.name AS to_name FROM economy_transfers t "
		"LEFT JOIN charinfo cf ON cf.id = t.from_character LEFT JOIN charinfo ct ON ct.id = t.to_character" + where + " ORDER BY t.id DESC LIMIT ? OFFSET ?;",
		length, start);
	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		data.push_back({
			{"time", result.getInt64Field("time")}, {"method", result.getIntField("method")},
			{"item_id", std::to_string(result.getInt64Field("item_id"))}, {"lot", result.getIntField("lot")},
			{"count", result.getIntField("count")}, {"coins", result.getInt64Field("coins")},
			{"from_character", std::to_string(result.getInt64Field("from_character"))},
			{"from_name", result.fieldIsNull("from_name") ? "" : result.getStringField("from_name")},
			{"to_character", std::to_string(result.getInt64Field("to_character"))},
			{"to_name", result.fieldIsNull("to_name") ? "" : result.getStringField("to_name")}
		});
		result.nextRow();
	}
	return { {"draw", 0}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} };
}

uint32_t SQLiteDatabase::PruneLog(eLog log, int64_t beforeTime) {
	switch (log) {
	case eLog::ACTIVITY: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM activity_log WHERE time > 0 AND time < ?;", beforeTime));
	case eLog::COMMAND: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM command_log WHERE time > 0 AND time < ?;", beforeTime));
	case eLog::AUDIT: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM audit_log WHERE timestamp > 0 AND timestamp < ?;", beforeTime));
	case eLog::CHEAT_DETECTION: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM player_cheat_detections WHERE violation_time < datetime(?, 'unixepoch');", beforeTime));
	case eLog::CHAT: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM chat_log WHERE time > 0 AND time < ?;", beforeTime));
	case eLog::LOGIN_ADDRESS: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM account_login_addresses WHERE last_seen < ?;", beforeTime));
	}
	return 0;
}
