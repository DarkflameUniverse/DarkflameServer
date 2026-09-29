#include "MySQLDatabase.h"

#include "GeneralUtils.h"
#include "json.hpp"

// Queries backing the web dashboard's moderation features.

namespace {
	constexpr const char* SNAPSHOT_QUERY =
		"SELECT "
		"(SELECT COUNT(*) FROM accounts) AS accounts, "
		"(SELECT COALESCE(MAX(id), 0) FROM accounts) AS accounts_max_id, "
		"(SELECT COUNT(*) FROM charinfo) AS characters, "
		"(SELECT COUNT(*) FROM charinfo WHERE pending_name != '' AND needs_rename = 0) AS pending_names, "
		"(SELECT COUNT(*) FROM properties) AS properties, "
		"(SELECT COUNT(*) FROM properties WHERE mod_approved = 0 AND privacy_option = 2 AND rejection_reason = '') AS pending_properties, "
		"(SELECT COUNT(*) FROM play_keys) AS play_keys, "
		"(SELECT COUNT(*) FROM bug_reports) AS bug_reports, "
		"(SELECT COUNT(*) FROM bug_reports WHERE resolved_time IS NULL) AS unresolved_bug_reports, "
		"(SELECT COUNT(*) FROM pet_names) AS pet_names, "
		"(SELECT COUNT(*) FROM pet_names WHERE approved = 1) AS pending_pet_names, "
		"(SELECT COALESCE(MAX(id), 0) FROM activity_log) AS activity_log_max_id, "
		"(SELECT COALESCE(MAX(id), 0) FROM chat_log) AS chat_log_max_id, "
		"(SELECT COALESCE(MAX(id), 0) FROM command_log) AS command_log_max_id, "
		"(SELECT COALESCE(MAX(id), 0) FROM audit_log) AS audit_log_max_id, "
		"(SELECT COALESCE(MAX(id), 0) FROM mail) AS mail_max_id, "
		"(SELECT COUNT(*) FROM economy_flags WHERE status = 0) AS open_economy_flags, "
		"(SELECT COALESCE(MAX(id), 0) FROM economy_flags) AS economy_flags_max_id;";
}

namespace {
	std::string NullableString(PreparedStmtResultSet& result, const char* field) {
		return result->isNull(field) ? "" : result->getString(field).c_str();
	}
}

std::string MySQLDatabase::GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc, int8_t resolvedFilter) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	const std::string from = " FROM bug_reports b LEFT JOIN charinfo c ON c.id = b.reporter_id";

	std::string resolvedCondition;
	if (resolvedFilter == 0) resolvedCondition = "b.resolved_time IS NULL";
	else if (resolvedFilter == 1) resolvedCondition = "b.resolved_time IS NOT NULL";

	std::string where = resolvedCondition.empty() ? "" : " WHERE " + resolvedCondition;
	const std::string totalWhere = where;
	if (!search.empty()) {
		where += (where.empty() ? " WHERE " : " AND ");
		where += "(b.body LIKE CONCAT('%', ?, '%') OR b.other_player_id LIKE CONCAT('%', ?, '%') OR c.name LIKE CONCAT('%', ?, '%') OR b.id = ? OR b.reporter_id = ?)";
	}

	std::string orderColumnName = "b.id";
	switch (orderColumn) {
		case 1: orderColumnName = "reporter_name"; break;
		case 2: orderColumnName = "b.client_version"; break;
		case 3: orderColumnName = "b.submitted"; break;
		case 4: orderColumnName = "b.resolved_time"; break;
		default: orderColumnName = "b.id"; break;
	}

	auto totalResult = ExecuteSelect("SELECT COUNT(*) AS count" + from + totalWhere + ";");
	const uint32_t totalRecords = totalResult->next() ? totalResult->getUInt("count") : 0;

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto filteredResult = ExecuteSelect("SELECT COUNT(*) AS count" + from + where + ";", search, search, search, searchId, searchId);
		filteredRecords = filteredResult->next() ? filteredResult->getUInt("count") : 0;
	}

	const std::string query =
		"SELECT b.id, b.body, b.client_version, b.other_player_id, b.selection, b.submitted, b.reporter_id, c.name AS reporter_name, b.resolved_time"
		+ from + where + " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + " LIMIT ?, ?;";
	auto result = !search.empty()
		? ExecuteSelect(query, search, search, search, searchId, searchId, start, length)
		: ExecuteSelect(query, start, length);

	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		data.push_back({
			{"id", result->getInt("id")},
			{"body", result->getString("body").c_str()},
			{"client_version", result->getString("client_version").c_str()},
			{"other_player_id", result->getString("other_player_id").c_str()},
			{"selection", result->getString("selection").c_str()},
			{"submitted", result->getString("submitted").c_str()},
			{"reporter_id", std::to_string(result->getInt64("reporter_id"))},
			{"reporter_name", NullableString(result, "reporter_name")},
			{"resolved", !result->isNull("resolved_time")}
		});
	}

	return nlohmann::json({ {"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", data} }).dump();
}

nlohmann::json MySQLDatabase::GetBugReport(const uint32_t id) {
	auto result = ExecuteSelect(
		"SELECT b.id, b.body, b.client_version, b.other_player_id, b.selection, b.submitted, b.reporter_id, c.name AS reporter_name, "
		"b.resolved_time, b.resolution, b.resoleved_by_id, a.name AS resolver_name "
		"FROM bug_reports b LEFT JOIN charinfo c ON c.id = b.reporter_id LEFT JOIN accounts a ON a.id = b.resoleved_by_id "
		"WHERE b.id = ? LIMIT 1;", id);
	if (!result->next()) return nlohmann::json{ {"error", "Bug report not found"} };

	return nlohmann::json{
		{"id", result->getInt("id")},
		{"body", result->getString("body").c_str()},
		{"client_version", result->getString("client_version").c_str()},
		{"other_player_id", result->getString("other_player_id").c_str()},
		{"selection", result->getString("selection").c_str()},
		{"submitted", result->getString("submitted").c_str()},
		{"reporter_id", std::to_string(result->getInt64("reporter_id"))},
		{"reporter_name", NullableString(result, "reporter_name")},
		{"resolved", !result->isNull("resolved_time")},
		{"resolved_time", NullableString(result, "resolved_time")},
		{"resolution", NullableString(result, "resolution")},
		{"resolver_name", NullableString(result, "resolver_name")}
	};
}

void MySQLDatabase::ResolveBugReport(const uint32_t id, const uint32_t resolverAccountId, const std::string_view resolution) {
	ExecuteUpdate("UPDATE bug_reports SET resolved_time = NOW(), resoleved_by_id = ?, resolution = ? WHERE id = ?;", resolverAccountId, resolution, id);
}

std::string MySQLDatabase::GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	const std::string where = search.empty() ? "" : " WHERE (p.key_string LIKE CONCAT('%', ?, '%') OR p.notes LIKE CONCAT('%', ?, '%') OR p.id = ? OR p.id IN (SELECT play_key_id FROM accounts WHERE name LIKE CONCAT('%', ?, '%')))";

	std::string orderColumnName = "p.id";
	switch (orderColumn) {
		case 1: orderColumnName = "p.key_string"; break;
		case 2: orderColumnName = "p.key_uses"; break;
		case 3: orderColumnName = "times_used"; break;
		case 4: orderColumnName = "p.created_at"; break;
		case 5: orderColumnName = "p.active"; break;
		default: orderColumnName = "p.id"; break;
	}

	auto totalResult = ExecuteSelect("SELECT COUNT(*) AS count FROM play_keys;");
	const uint32_t totalRecords = totalResult->next() ? totalResult->getUInt("count") : 0;

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto filteredResult = ExecuteSelect("SELECT COUNT(*) AS count FROM play_keys p" + where + ";", search, search, searchId, search);
		filteredRecords = filteredResult->next() ? filteredResult->getUInt("count") : 0;
	}

	const std::string query =
		"SELECT p.id, p.key_string, p.key_uses, p.created_at, p.active, p.notes, "
		"(SELECT COUNT(*) FROM accounts a WHERE a.play_key_id = p.id) AS times_used FROM play_keys p"
		+ where + " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + " LIMIT ?, ?;";
	auto result = !search.empty()
		? ExecuteSelect(query, search, search, searchId, search, start, length)
		: ExecuteSelect(query, start, length);

	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		data.push_back({
			{"id", result->getInt("id")},
			{"key_string", result->getString("key_string").c_str()},
			{"key_uses", result->getInt("key_uses")},
			{"times_used", result->getInt("times_used")},
			{"created_at", result->getString("created_at").c_str()},
			{"active", result->getInt("active") != 0},
			{"notes", NullableString(result, "notes")}
		});
	}

	return nlohmann::json({ {"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", data} }).dump();
}

nlohmann::json MySQLDatabase::GetPlayKey(const int32_t playkeyId) {
	auto result = ExecuteSelect("SELECT id, key_string, key_uses, created_at, active, notes FROM play_keys WHERE id = ? LIMIT 1;", playkeyId);
	if (!result->next()) return nlohmann::json{ {"error", "Play key not found"} };

	nlohmann::json key{
		{"id", result->getInt("id")},
		{"key_string", result->getString("key_string").c_str()},
		{"key_uses", result->getInt("key_uses")},
		{"created_at", result->getString("created_at").c_str()},
		{"active", result->getInt("active") != 0},
		{"notes", NullableString(result, "notes")}
	};

	nlohmann::json accounts = nlohmann::json::array();
	auto accountResult = ExecuteSelect("SELECT id, name, created_at FROM accounts WHERE play_key_id = ? ORDER BY id;", playkeyId);
	while (accountResult->next()) {
		accounts.push_back({
			{"id", accountResult->getInt("id")},
			{"name", accountResult->getString("name").c_str()},
			{"created_at", accountResult->getString("created_at").c_str()}
		});
	}
	key["times_used"] = accounts.size();
	key["accounts"] = accounts;
	return key;
}

void MySQLDatabase::UpdatePlayKey(const int32_t playkeyId, const uint32_t uses, const std::string_view notes, const bool active) {
	ExecuteUpdate("UPDATE play_keys SET key_uses = ?, notes = ?, active = ? WHERE id = ?;", uses, notes, active ? 1 : 0, playkeyId);
}

void MySQLDatabase::DeletePlayKey(const int32_t playkeyId) {
	ExecuteUpdate("UPDATE accounts SET play_key_id = NULL WHERE play_key_id = ?;", playkeyId);
	ExecuteDelete("DELETE FROM play_keys WHERE id = ?;", playkeyId);
}

std::vector<std::pair<LWOOBJID, std::string>> MySQLDatabase::GetCharacterIdsAndNames() {
	auto result = ExecuteSelect("SELECT id, name FROM charinfo ORDER BY name;");
	std::vector<std::pair<LWOOBJID, std::string>> characters;
	while (result->next()) {
		characters.emplace_back(result->getInt64("id"), std::string(result->getString("name").c_str()));
	}
	return characters;
}

nlohmann::json MySQLDatabase::GetPendingNamesTable(const uint32_t start, const uint32_t length) {
	auto countResult = ExecuteSelect("SELECT COUNT(*) AS count FROM charinfo WHERE pending_name != '' AND needs_rename = 0;");
	const uint32_t total = countResult->next() ? countResult->getUInt("count") : 0;

	auto result = ExecuteSelect(
		"SELECT c.id, c.name, c.pending_name, c.account_id, a.name AS account_name "
		"FROM charinfo c LEFT JOIN accounts a ON a.id = c.account_id "
		"WHERE c.pending_name != '' AND c.needs_rename = 0 ORDER BY c.id LIMIT ?, ?;", start, length);

	nlohmann::json data = nlohmann::json::array();
	while (result->next()) {
		data.push_back({
			{"id", std::to_string(result->getInt64("id"))},
			{"name", result->getString("name").c_str()},
			{"pending_name", result->getString("pending_name").c_str()},
			{"account_id", result->getInt("account_id")},
			{"account_name", NullableString(result, "account_name")}
		});
	}
	return nlohmann::json{ {"draw", 0}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} };
}

void MySQLDatabase::SetCharacterPermissionMap(const LWOOBJID characterId, const uint64_t permissionMap) {
	ExecuteUpdate("UPDATE charinfo SET permission_map = ? WHERE id = ?;", permissionMap, characterId);
}

IDashboardStats::Snapshot MySQLDatabase::GetDashboardSnapshot() {
	IDashboardStats::Snapshot snapshot;
	auto result = ExecuteSelect(SNAPSHOT_QUERY);
	if (!result->next()) return snapshot;
	snapshot.accounts = result->getUInt64("accounts");
	snapshot.accountsMaxId = result->getUInt64("accounts_max_id");
	snapshot.characters = result->getUInt64("characters");
	snapshot.pendingNames = result->getUInt64("pending_names");
	snapshot.properties = result->getUInt64("properties");
	snapshot.pendingProperties = result->getUInt64("pending_properties");
	snapshot.playKeys = result->getUInt64("play_keys");
	snapshot.bugReports = result->getUInt64("bug_reports");
	snapshot.unresolvedBugReports = result->getUInt64("unresolved_bug_reports");
	snapshot.petNames = result->getUInt64("pet_names");
	snapshot.pendingPetNames = result->getUInt64("pending_pet_names");
	snapshot.activityLogMaxId = result->getUInt64("activity_log_max_id");
	snapshot.chatLogMaxId = result->getUInt64("chat_log_max_id");
	snapshot.commandLogMaxId = result->getUInt64("command_log_max_id");
	snapshot.auditLogMaxId = result->getUInt64("audit_log_max_id");
	snapshot.mailMaxId = result->getUInt64("mail_max_id");
	snapshot.openEconomyFlags = result->getUInt64("open_economy_flags");
	snapshot.economyFlagsMaxId = result->getUInt64("economy_flags_max_id");
	return snapshot;
}

std::optional<LWOOBJID> MySQLDatabase::GetModelPropertyId(const LWOOBJID modelID) {
	auto result = ExecuteSelect("SELECT property_id FROM properties_contents WHERE id = ? LIMIT 1;", modelID);
	if (!result->next()) return std::nullopt;
	return result->getInt64("property_id");
}

std::optional<int32_t> MySQLDatabase::GetRedeemablePlayKeyId(const std::string_view keyString) {
	auto result = ExecuteSelect(
		"SELECT p.id FROM play_keys p WHERE p.key_string = ? AND p.active = 1 "
		"AND (SELECT COUNT(*) FROM accounts a WHERE a.play_key_id = p.id) < p.key_uses LIMIT 1;", keyString);
	if (!result->next()) return std::nullopt;
	return result->getInt("id");
}

void MySQLDatabase::SetAccountPlayKey(const uint32_t accountId, const int32_t playkeyId) {
	ExecuteUpdate("UPDATE accounts SET play_key_id = ? WHERE id = ?;", playkeyId, accountId);
}

std::optional<IAccountEmails::EmailInfo> MySQLDatabase::GetAccountEmail(const uint32_t accountId) {
	auto result = ExecuteSelect("SELECT email, email_confirmed_at FROM accounts WHERE id = ? LIMIT 1;", accountId);
	if (!result->next()) return std::nullopt;
	return IAccountEmails::EmailInfo{ result->isNull("email") ? "" : std::string(result->getString("email").c_str()), !result->isNull("email_confirmed_at") };
}

void MySQLDatabase::SetAccountEmail(const uint32_t accountId, const std::string_view email, const bool confirmed) {
	if (confirmed) ExecuteUpdate("UPDATE accounts SET email = ?, email_confirmed_at = CURRENT_TIMESTAMP WHERE id = ?;", email, accountId);
	else ExecuteUpdate("UPDATE accounts SET email = ?, email_confirmed_at = NULL WHERE id = ?;", email, accountId);
}

std::optional<uint32_t> MySQLDatabase::GetAccountIdByConfirmedEmail(const std::string_view email) {
	auto result = ExecuteSelect("SELECT id FROM accounts WHERE LOWER(email) = LOWER(?) AND email_confirmed_at IS NOT NULL LIMIT 1;", email);
	if (!result->next()) return std::nullopt;
	return static_cast<uint32_t>(result->getUInt("id"));
}

void MySQLDatabase::InsertAccountToken(const std::string_view tokenHash, const uint32_t accountId, const std::string_view purpose, const std::string_view data, const int64_t expiresAt) {
	ExecuteInsert("INSERT INTO account_tokens (token_hash, account_id, purpose, data, expires_at) VALUES (?, ?, ?, ?, ?);", tokenHash, accountId, purpose, data, expiresAt);
}

std::optional<IAccountEmails::AccountToken> MySQLDatabase::ConsumeAccountToken(const std::string_view tokenHash, const std::string_view purpose) {
	std::optional<IAccountEmails::AccountToken> token;
	{
		auto result = ExecuteSelect("SELECT account_id, data FROM account_tokens WHERE token_hash = ? AND purpose = ? AND expires_at > ? LIMIT 1;",
			tokenHash, purpose, static_cast<int64_t>(std::time(nullptr)));
		if (result->next()) token = IAccountEmails::AccountToken{ result->getUInt("account_id"), std::string(result->getString("data").c_str()) };
	}
	ExecuteDelete("DELETE FROM account_tokens WHERE token_hash = ?;", tokenHash);
	return token;
}

void MySQLDatabase::DeleteAccountTokens(const uint32_t accountId, const std::string_view purpose) {
	ExecuteDelete("DELETE FROM account_tokens WHERE account_id = ? AND purpose = ?;", accountId, purpose);
}

void MySQLDatabase::DeleteExpiredAccountTokens() {
	ExecuteDelete("DELETE FROM account_tokens WHERE expires_at <= ?;", static_cast<int64_t>(std::time(nullptr)));
}

int64_t MySQLDatabase::GetSessionsValidAfter(const uint32_t accountId) {
	auto result = ExecuteSelect("SELECT sessions_valid_after FROM accounts WHERE id = ? LIMIT 1;", accountId);
	return result->next() ? result->getInt64("sessions_valid_after") : 0;
}

void MySQLDatabase::SetSessionsValidAfter(const uint32_t accountId, const int64_t time) {
	ExecuteUpdate("UPDATE accounts SET sessions_valid_after = ? WHERE id = ?;", time, accountId);
}

uint32_t MySQLDatabase::ApprovePreviouslyApprovedPetNames() {
	// The derived table is needed because MySQL can't select from the table being updated
	return static_cast<uint32_t>(ExecuteUpdate(
		"UPDATE pet_names SET approved = 2 WHERE approved = 1 AND pet_name IN "
		"(SELECT name FROM (SELECT pet_name AS name FROM pet_names WHERE approved = 2) AS approved_names);"));
}

void MySQLDatabase::ForEachCharacterXml(const std::function<void(LWOOBJID, const std::string&)>& visit) {
	auto result = ExecuteSelect("SELECT id, xml_data FROM charxml;");
	while (result->next()) visit(result->getInt64("id"), std::string(result->getString("xml_data").c_str()));
}

void MySQLDatabase::ForEachCharacterXmlContaining(const std::string& needle, const std::function<void(LWOOBJID, const std::string&)>& visit) {
	auto result = ExecuteSelect("SELECT id, xml_data FROM charxml WHERE LOCATE(?, xml_data) > 0;", needle);
	while (result->next()) visit(result->getInt64("id"), std::string(result->getString("xml_data").c_str()));
}
