#include "MySQLDatabase.h"

#include <ctime>

namespace {
	std::string Text(PreparedStmtResultSet& result, const char* field) {
		return result->isNull(field) ? "" : std::string(result->getString(field).c_str());
	}

	IDashboardAdmin::Webhook WebhookRow(PreparedStmtResultSet& result) {
		IDashboardAdmin::Webhook webhook;
		webhook.id = result->getUInt("id");
		webhook.name = Text(result, "name");
		webhook.url = Text(result, "url");
		webhook.format = Text(result, "format");
		webhook.events = Text(result, "events");
		webhook.secret = Text(result, "secret");
		webhook.enabled = result->getInt("enabled") != 0;
		webhook.createdAt = result->getInt64("created_at");
		webhook.lastSentAt = result->getInt64("last_sent_at");
		webhook.lastStatus = result->getInt("last_status");
		webhook.lastError = Text(result, "last_error");
		return webhook;
	}

	// A ledger table: its key columns (besides day) and the value columns that are summed
	struct LedgerTable {
		const char* name;
		const char* keys;
		std::vector<const char*> values;
	};

	const std::vector<LedgerTable> DAILY_TABLES{
		{ "economy_currency_daily", "character_id, source", { "gained", "spent" } },
		{ "economy_uscore_daily", "character_id, source", { "gained", "lost" } },
		{ "economy_items_daily", "lot, source, gm", { "created", "destroyed" } },
		{ "player_stats_daily", "zone, clone_id, stat, gm", { "amount" } },
	};
	const LedgerTable MAP_TABLE{ "map_events_daily", "zone, clone_id, kind, lot, cell_x, cell_z", { "events", "quantity" } };
}

std::vector<IDashboardAdmin::Webhook> MySQLDatabase::GetWebhooks() {
	auto result = ExecuteSelect("SELECT * FROM dashboard_webhooks ORDER BY id;");
	std::vector<Webhook> webhooks;
	while (result->next()) webhooks.push_back(WebhookRow(result));
	return webhooks;
}

std::optional<IDashboardAdmin::Webhook> MySQLDatabase::GetWebhook(uint32_t id) {
	auto result = ExecuteSelect("SELECT * FROM dashboard_webhooks WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return WebhookRow(result);
}

void MySQLDatabase::InsertWebhook(const Webhook& webhook) {
	ExecuteInsert("INSERT INTO dashboard_webhooks (name, url, format, events, secret, enabled, created_at) VALUES (?, ?, ?, ?, ?, ?, ?);",
		webhook.name, webhook.url, webhook.format, webhook.events, webhook.secret, webhook.enabled, static_cast<int64_t>(std::time(nullptr)));
}

void MySQLDatabase::UpdateWebhook(const Webhook& webhook) {
	ExecuteUpdate("UPDATE dashboard_webhooks SET name = ?, url = ?, format = ?, events = ?, secret = ?, enabled = ? WHERE id = ?;",
		webhook.name, webhook.url, webhook.format, webhook.events, webhook.secret, webhook.enabled, webhook.id);
}

void MySQLDatabase::DeleteWebhook(uint32_t id) {
	ExecuteDelete("DELETE FROM dashboard_webhooks WHERE id = ?;", id);
}

void MySQLDatabase::RecordWebhookResult(uint32_t id, int64_t time, int32_t status, const std::string& error) {
	ExecuteUpdate("UPDATE dashboard_webhooks SET last_sent_at = ?, last_status = ?, last_error = ? WHERE id = ?;", time, status, error, id);
}

std::optional<std::string> MySQLDatabase::GetDashboardState(const std::string& name) {
	auto result = ExecuteSelect("SELECT value FROM dashboard_state WHERE name = ?;", name);
	if (!result->next()) return std::nullopt;
	return Text(result, "value");
}

void MySQLDatabase::SetDashboardState(const std::string& name, const std::string& value) {
	ExecuteInsert("INSERT INTO dashboard_state (name, value) VALUES (?, ?) ON DUPLICATE KEY UPDATE value = VALUES(value);", name, value);
}

void MySQLDatabase::DeleteDashboardState(const std::string& name) {
	ExecuteDelete("DELETE FROM dashboard_state WHERE name = ?;", name);
}

std::string MySQLDatabase::GetDashboardPreferences(uint32_t accountId) {
	auto result = ExecuteSelect("SELECT prefs FROM dashboard_preferences WHERE account_id = ?;", accountId);
	if (!result->next()) return "{}";
	return Text(result, "prefs");
}

void MySQLDatabase::SetDashboardPreferences(uint32_t accountId, const std::string& prefs) {
	ExecuteInsert("INSERT INTO dashboard_preferences (account_id, prefs) VALUES (?, ?) ON DUPLICATE KEY UPDATE prefs = VALUES(prefs);", accountId, prefs);
}

bool MySQLDatabase::InsertEconomyFlag(const EconomyFlag& flag) {
	return ExecuteUpdate(
		"INSERT IGNORE INTO economy_flags (created_at, day, kind, character_id, lot, item_id, value, baseline, details) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
		static_cast<int64_t>(std::time(nullptr)), flag.day, static_cast<uint32_t>(flag.kind), flag.characterId, flag.lot, flag.itemId,
		flag.value, flag.baseline, flag.details) > 0;
}

nlohmann::json MySQLDatabase::GetEconomyFlagsTable(uint32_t start, uint32_t length, int32_t status) {
	const std::string where = status >= 0 ? " WHERE f.status = ?" : "";
	uint32_t count = 0;
	{
		auto result = status >= 0 ? ExecuteSelect("SELECT COUNT(*) AS count FROM economy_flags f" + where + ";", status)
			: ExecuteSelect("SELECT COUNT(*) AS count FROM economy_flags f;");
		if (result->next()) count = result->getUInt("count");
	}
	const std::string query = "SELECT f.*, c.name AS character_name, a.name AS reviewer_name FROM economy_flags f "
		"LEFT JOIN charinfo c ON c.id = f.character_id LEFT JOIN accounts a ON a.id = f.reviewed_by" + where + " ORDER BY f.id DESC LIMIT ? OFFSET ?;";
	auto result = status >= 0 ? ExecuteSelect(query, status, length, start) : ExecuteSelect(query, length, start);
	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		data.push_back({
			{"id", result->getInt64("id")},
			{"created_at", result->getInt64("created_at")},
			{"day", result->getInt("day")},
			{"kind", result->getInt("kind")},
			{"character_id", std::to_string(result->getInt64("character_id"))},
			{"character_name", Text(result, "character_name")},
			{"lot", result->getInt("lot")},
			{"item_id", std::to_string(result->getInt64("item_id"))},
			{"value", result->getInt64("value")},
			{"baseline", result->getInt64("baseline")},
			{"details", Text(result, "details")},
			{"status", result->getInt("status")},
			{"reviewer_name", Text(result, "reviewer_name")},
			{"reviewed_at", result->getInt64("reviewed_at")},
			{"note", Text(result, "note")}
		});
	}
	return { {"draw", 0}, {"recordsTotal", count}, {"recordsFiltered", count}, {"data", data} };
}

void MySQLDatabase::ReviewEconomyFlag(uint64_t id, eFlagStatus status, uint32_t reviewerAccountId, const std::string& note) {
	ExecuteUpdate("UPDATE economy_flags SET status = ?, reviewed_by = ?, reviewed_at = ?, note = ? WHERE id = ?;",
		static_cast<uint32_t>(status), reviewerAccountId, static_cast<int64_t>(std::time(nullptr)), note, id);
}

uint32_t MySQLDatabase::GetOpenEconomyFlagCount() {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM economy_flags WHERE status = 0;");
	return result->next() ? result->getUInt("count") : 0;
}

std::vector<std::pair<LWOOBJID, int64_t>> MySQLDatabase::GetDailyIncome(uint32_t day) {
	auto result = ExecuteSelect(
		"SELECT f.character_id, SUM(f.gained) AS gained FROM economy_currency_daily f "
		"LEFT JOIN charinfo c ON c.id = f.character_id LEFT JOIN accounts a ON a.id = c.account_id "
		"WHERE f.day = ? AND f.source NOT IN (3, 6) AND COALESCE(a.gm_level, 0) < 3 GROUP BY f.character_id HAVING SUM(f.gained) > 0;", day);
	std::vector<std::pair<LWOOBJID, int64_t>> rows;
	while (result->next()) rows.emplace_back(result->getInt64("character_id"), result->getInt64("gained"));
	return rows;
}

std::map<LOT, int64_t> MySQLDatabase::GetItemCreationTotals(uint32_t fromDay, uint32_t toDay) {
	auto result = ExecuteSelect(
		"SELECT lot, SUM(created) AS created FROM economy_items_daily WHERE day BETWEEN ? AND ? AND gm = 0 GROUP BY lot;", fromDay, toDay);
	std::map<LOT, int64_t> totals;
	while (result->next()) totals[result->getInt("lot")] = result->getInt64("created");
	return totals;
}

uint32_t MySQLDatabase::CompactEconomy(uint32_t cutoffDay, uint32_t mapCutoffDay) {
	uint32_t merged = 0;
	const auto compact = [&](const LedgerTable& table, uint32_t cutoff) {
		std::vector<uint32_t> days;
		{
			auto result = ExecuteSelect(std::string("SELECT DISTINCT day FROM ") + table.name + " WHERE day < ?;", cutoff);
			while (result->next()) days.push_back(result->getUInt("day"));
		}
		std::string values, selectValues, sums;
		for (const auto* value : table.values) {
			values += std::string(", ") + value;
			selectValues += std::string(", s.") + value;
			sums += std::string(sums.empty() ? "" : ", ") + value + " = " + table.name + "." + value + " + VALUES(" + value + ")";
		}
		std::string selectKeys;
		for (const auto& key : GeneralUtils::SplitString(table.keys, ',')) {
			auto trimmed = key;
			trimmed.erase(0, trimmed.find_first_not_of(' '));
			selectKeys += std::string(selectKeys.empty() ? "" : ", ") + "s." + trimmed;
		}
		for (const auto day : days) {
			const auto month = MonthStartDay(day);
			if (month == day) continue;
			// MySQL buffers a SELECT from the table being inserted into, so this is safe
			// One short transaction per day, so row locks are never held for the whole compaction
			DatabaseTransaction transaction(*this);
			ExecuteInsert(std::string("INSERT INTO ") + table.name + " (day, " + table.keys + values + ") SELECT ?, " + selectKeys + selectValues +
				" FROM " + table.name + " s WHERE s.day = ? ON DUPLICATE KEY UPDATE " + sums + ";", month, day);
			const auto deleted = static_cast<uint32_t>(ExecuteUpdate(std::string("DELETE FROM ") + table.name + " WHERE day = ?;", day));
			transaction.Commit();
			merged += deleted;
		}
	};
	for (const auto& table : DAILY_TABLES) compact(table, cutoffDay);
	compact(MAP_TABLE, mapCutoffDay);
	return merged;
}

uint32_t MySQLDatabase::PruneTransfers(int64_t beforeTime) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM economy_transfers WHERE time < ?;", beforeTime));
}

IDashboardAdmin::Totp MySQLDatabase::GetTotp(uint32_t accountId) {
	Totp totp;
	auto result = ExecuteSelect("SELECT totp_secret, totp_enabled_at, totp_last_step FROM accounts WHERE id = ?;", accountId);
	if (!result->next()) return totp;
	totp.encryptedSecret = Text(result, "totp_secret");
	totp.enabledAt = result->getInt64("totp_enabled_at");
	totp.lastStep = result->getInt64("totp_last_step");
	return totp;
}

void MySQLDatabase::SetTotp(uint32_t accountId, const std::string& encryptedSecret, int64_t enabledAt) {
	ExecuteUpdate("UPDATE accounts SET totp_secret = ?, totp_enabled_at = ?, totp_last_step = 0 WHERE id = ?;", encryptedSecret, enabledAt, accountId);
}

bool MySQLDatabase::UseTotpStep(uint32_t accountId, int64_t step) {
	return ExecuteUpdate("UPDATE accounts SET totp_last_step = ? WHERE id = ? AND totp_last_step < ?;", step, accountId, step) > 0;
}

void MySQLDatabase::ReplaceRecoveryCodes(uint32_t accountId, const std::vector<std::string>& codeHashes) {
	ExecuteDelete("DELETE FROM account_recovery_codes WHERE account_id = ?;", accountId);
	for (const auto& hash : codeHashes) ExecuteInsert("INSERT INTO account_recovery_codes (account_id, code_hash) VALUES (?, ?);", accountId, hash);
}

bool MySQLDatabase::UseRecoveryCode(uint32_t accountId, const std::string& codeHash) {
	return ExecuteUpdate("UPDATE account_recovery_codes SET used_at = ? WHERE account_id = ? AND code_hash = ? AND used_at = 0;",
		static_cast<int64_t>(std::time(nullptr)), accountId, codeHash) > 0;
}

uint32_t MySQLDatabase::GetRecoveryCodesLeft(uint32_t accountId) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM account_recovery_codes WHERE account_id = ? AND used_at = 0;", accountId);
	return result->next() ? result->getUInt("count") : 0;
}

nlohmann::json MySQLDatabase::GetBugReportsAfter(uint32_t afterId, uint32_t limit) {
	auto result = ExecuteSelect(
		"SELECT b.id, b.body, b.client_version, b.other_player_id, b.selection, b.reporter_id, c.name AS reporter_name FROM bug_reports b "
		"LEFT JOIN charinfo c ON c.id = b.reporter_id WHERE b.id > ? ORDER BY b.id LIMIT ?;", afterId, limit);
	nlohmann::json rows = nlohmann::json::array();
	while (result->next()) {
		rows.push_back({
			{"id", result->getInt("id")}, {"body", Text(result, "body")}, {"client_version", Text(result, "client_version")},
			{"other_player_id", Text(result, "other_player_id")}, {"selection", Text(result, "selection")},
			{"reporter_id", std::to_string(result->getInt64("reporter_id"))}, {"reporter_name", Text(result, "reporter_name")}
		});
	}
	return rows;
}

uint32_t MySQLDatabase::GetMaxBugReportId() {
	auto result = ExecuteSelect("SELECT COALESCE(MAX(id), 0) AS id FROM bug_reports;");
	return result->next() ? result->getUInt("id") : 0;
}

nlohmann::json MySQLDatabase::GetTransfersForCharacters(const std::vector<LWOOBJID>& characterIds, uint32_t start, uint32_t length) {
	nlohmann::json empty{ {"draw", 0}, {"recordsTotal", 0}, {"recordsFiltered", 0}, {"data", nlohmann::json::array()} };
	if (characterIds.empty()) return empty;
	// Ids are numbers from the database, so listing them inline is safe
	std::string list;
	for (const auto id : characterIds) list += (list.empty() ? "" : ",") + std::to_string(id);
	const std::string where = " WHERE (t.from_character IN (" + list + ") OR t.to_character IN (" + list + ")) AND t.method <> " +
		std::to_string(static_cast<uint32_t>(IEconomyLedger::eTransferMethod::INVENTORY_MOVE));

	uint32_t total = 0;
	{
		auto countResult = ExecuteSelect("SELECT COUNT(*) AS count FROM economy_transfers t" + where + ";");
		if (countResult->next()) total = countResult->getUInt("count");
	}
	auto result = ExecuteSelect(
		"SELECT t.*, cf.name AS from_name, ct.name AS to_name FROM economy_transfers t "
		"LEFT JOIN charinfo cf ON cf.id = t.from_character LEFT JOIN charinfo ct ON ct.id = t.to_character" + where + " ORDER BY t.id DESC LIMIT ? OFFSET ?;",
		length, start);
	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		data.push_back({
			{"time", result->getInt64("time")}, {"method", result->getInt("method")},
			{"item_id", std::to_string(result->getInt64("item_id"))}, {"lot", result->getInt("lot")},
			{"count", result->getInt("count")}, {"coins", result->getInt64("coins")},
			{"from_character", std::to_string(result->getInt64("from_character"))}, {"from_name", Text(result, "from_name")},
			{"to_character", std::to_string(result->getInt64("to_character"))}, {"to_name", Text(result, "to_name")}
		});
	}
	return { {"draw", 0}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} };
}

uint32_t MySQLDatabase::PruneLog(eLog log, int64_t beforeTime) {
	switch (log) {
	case eLog::ACTIVITY: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM activity_log WHERE time > 0 AND time < ?;", beforeTime));
	case eLog::COMMAND: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM command_log WHERE time > 0 AND time < ?;", beforeTime));
	case eLog::AUDIT: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM audit_log WHERE timestamp > 0 AND timestamp < ?;", beforeTime));
	case eLog::CHEAT_DETECTION: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM player_cheat_detections WHERE violation_time < FROM_UNIXTIME(?);", beforeTime));
	case eLog::CHAT: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM chat_log WHERE time > 0 AND time < ?;", beforeTime));
	case eLog::LOGIN_ADDRESS: return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM account_login_addresses WHERE last_seen < ?;", beforeTime));
	}
	return 0;
}
