#include "SQLiteDatabase.h"

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
	std::string NullableString(CppSQLite3Query& result, const char* field) {
		return result.fieldIsNull(field) ? "" : result.getStringField(field);
	}
}

std::string SQLiteDatabase::GetBugReportsTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc, int8_t resolvedFilter) {
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
		where += "(b.body LIKE '%' || ? || '%' OR b.other_player_id LIKE '%' || ? || '%' OR c.name LIKE '%' || ? || '%' OR b.id = ? OR b.reporter_id = ?)";
	}

	std::string orderColumnName = "b.id";
	switch (orderColumn) {
		case 1: orderColumnName = "reporter_name"; break;
		case 2: orderColumnName = "b.client_version"; break;
		case 3: orderColumnName = "b.submitted"; break;
		case 4: orderColumnName = "b.resolved_time"; break;
		default: orderColumnName = "b.id"; break;
	}

	auto [_, totalResult] = ExecuteSelect("SELECT COUNT(*) AS count" + from + totalWhere + ";");
	const uint32_t totalRecords = totalResult.eof() ? 0 : totalResult.getIntField("count");

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto [__, filteredResult] = ExecuteSelect("SELECT COUNT(*) AS count" + from + where + ";", search, search, search, searchId, searchId);
		filteredRecords = filteredResult.eof() ? 0 : filteredResult.getIntField("count");
	}

	const std::string query =
		"SELECT b.id, b.body, b.client_version, b.other_player_id, b.selection, b.submitted, b.reporter_id, c.name AS reporter_name, b.resolved_time"
		+ from + where + " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + " LIMIT ? OFFSET ?;";
	auto [stmt, result] = !search.empty()
		? ExecuteSelect(query, search, search, search, searchId, searchId, length, start)
		: ExecuteSelect(query, length, start);

	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		data.push_back({
			{"id", result.getIntField("id")},
			{"body", result.getStringField("body")},
			{"client_version", result.getStringField("client_version")},
			{"other_player_id", result.getStringField("other_player_id")},
			{"selection", result.getStringField("selection")},
			{"submitted", result.getStringField("submitted")},
			{"reporter_id", std::to_string(result.getInt64Field("reporter_id"))},
			{"reporter_name", NullableString(result, "reporter_name")},
			{"resolved", !result.fieldIsNull("resolved_time")}
		});
		result.nextRow();
	}

	return nlohmann::json({ {"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", data} }).dump();
}

nlohmann::json SQLiteDatabase::GetBugReport(const uint32_t id) {
	auto [_, result] = ExecuteSelect(
		"SELECT b.id, b.body, b.client_version, b.other_player_id, b.selection, b.submitted, b.reporter_id, c.name AS reporter_name, "
		"b.resolved_time, b.resolution, b.resoleved_by_id, a.name AS resolver_name "
		"FROM bug_reports b LEFT JOIN charinfo c ON c.id = b.reporter_id LEFT JOIN accounts a ON a.id = b.resoleved_by_id "
		"WHERE b.id = ? LIMIT 1;", id);
	if (result.eof()) return nlohmann::json{ {"error", "Bug report not found"} };

	return nlohmann::json{
		{"id", result.getIntField("id")},
		{"body", result.getStringField("body")},
		{"client_version", result.getStringField("client_version")},
		{"other_player_id", result.getStringField("other_player_id")},
		{"selection", result.getStringField("selection")},
		{"submitted", result.getStringField("submitted")},
		{"reporter_id", std::to_string(result.getInt64Field("reporter_id"))},
		{"reporter_name", NullableString(result, "reporter_name")},
		{"resolved", !result.fieldIsNull("resolved_time")},
		{"resolved_time", NullableString(result, "resolved_time")},
		{"resolution", NullableString(result, "resolution")},
		{"resolver_name", NullableString(result, "resolver_name")}
	};
}

void SQLiteDatabase::ResolveBugReport(const uint32_t id, const uint32_t resolverAccountId, const std::string_view resolution) {
	ExecuteUpdate("UPDATE bug_reports SET resolved_time = datetime('now'), resoleved_by_id = ?, resolution = ? WHERE id = ?;", resolverAccountId, resolution, id);
}

std::string SQLiteDatabase::GetPlayKeysTable(uint32_t start, uint32_t length, const std::string_view search, uint32_t orderColumn, bool orderAsc) {
	// A number in the search box also matches IDs exactly (-1 never matches)
	const int64_t searchId = GeneralUtils::TryParse<int64_t>(std::string(search)).value_or(-1);
	const std::string where = search.empty() ? "" : " WHERE (p.key_string LIKE '%' || ? || '%' OR p.notes LIKE '%' || ? || '%' OR p.id = ? OR p.id IN (SELECT play_key_id FROM accounts WHERE name LIKE '%' || ? || '%'))";

	std::string orderColumnName = "p.id";
	switch (orderColumn) {
		case 1: orderColumnName = "p.key_string"; break;
		case 2: orderColumnName = "p.key_uses"; break;
		case 3: orderColumnName = "times_used"; break;
		case 4: orderColumnName = "p.created_at"; break;
		case 5: orderColumnName = "p.active"; break;
		default: orderColumnName = "p.id"; break;
	}

	auto [_, totalResult] = ExecuteSelect("SELECT COUNT(*) AS count FROM play_keys;");
	const uint32_t totalRecords = totalResult.eof() ? 0 : totalResult.getIntField("count");

	uint32_t filteredRecords = totalRecords;
	if (!search.empty()) {
		auto [__, filteredResult] = ExecuteSelect("SELECT COUNT(*) AS count FROM play_keys p" + where + ";", search, search, searchId, search);
		filteredRecords = filteredResult.eof() ? 0 : filteredResult.getIntField("count");
	}

	const std::string query =
		"SELECT p.id, p.key_string, p.key_uses, p.created_at, p.active, p.notes, "
		"(SELECT COUNT(*) FROM accounts a WHERE a.play_key_id = p.id) AS times_used FROM play_keys p"
		+ where + " ORDER BY " + orderColumnName + (orderAsc ? " ASC" : " DESC") + " LIMIT ? OFFSET ?;";
	auto [stmt, result] = !search.empty()
		? ExecuteSelect(query, search, search, searchId, search, length, start)
		: ExecuteSelect(query, length, start);

	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		data.push_back({
			{"id", result.getIntField("id")},
			{"key_string", result.getStringField("key_string")},
			{"key_uses", result.getIntField("key_uses")},
			{"times_used", result.getIntField("times_used")},
			{"created_at", result.getStringField("created_at")},
			{"active", result.getIntField("active") != 0},
			{"notes", NullableString(result, "notes")}
		});
		result.nextRow();
	}

	return nlohmann::json({ {"draw", 0}, {"recordsTotal", totalRecords}, {"recordsFiltered", filteredRecords}, {"data", data} }).dump();
}

nlohmann::json SQLiteDatabase::GetPlayKey(const int32_t playkeyId) {
	auto [_, result] = ExecuteSelect("SELECT id, key_string, key_uses, created_at, active, notes FROM play_keys WHERE id = ? LIMIT 1;", playkeyId);
	if (result.eof()) return nlohmann::json{ {"error", "Play key not found"} };

	nlohmann::json key{
		{"id", result.getIntField("id")},
		{"key_string", result.getStringField("key_string")},
		{"key_uses", result.getIntField("key_uses")},
		{"created_at", result.getStringField("created_at")},
		{"active", result.getIntField("active") != 0},
		{"notes", NullableString(result, "notes")}
	};

	nlohmann::json accounts = nlohmann::json::array();
	auto [__, accountResult] = ExecuteSelect("SELECT id, name, created_at FROM accounts WHERE play_key_id = ? ORDER BY id;", playkeyId);
	while (!accountResult.eof()) {
		accounts.push_back({
			{"id", accountResult.getIntField("id")},
			{"name", accountResult.getStringField("name")},
			{"created_at", accountResult.getStringField("created_at")}
		});
		accountResult.nextRow();
	}
	key["times_used"] = accounts.size();
	key["accounts"] = accounts;
	return key;
}

void SQLiteDatabase::UpdatePlayKey(const int32_t playkeyId, const uint32_t uses, const std::string_view notes, const bool active) {
	ExecuteUpdate("UPDATE play_keys SET key_uses = ?, notes = ?, active = ? WHERE id = ?;", uses, notes, active ? 1 : 0, playkeyId);
}

void SQLiteDatabase::DeletePlayKey(const int32_t playkeyId) {
	ExecuteUpdate("UPDATE accounts SET play_key_id = NULL WHERE play_key_id = ?;", playkeyId);
	ExecuteDelete("DELETE FROM play_keys WHERE id = ?;", playkeyId);
}

std::vector<std::pair<LWOOBJID, std::string>> SQLiteDatabase::GetCharacterIdsAndNames() {
	auto [_, result] = ExecuteSelect("SELECT id, name FROM charinfo ORDER BY name;");
	std::vector<std::pair<LWOOBJID, std::string>> characters;
	while (!result.eof()) {
		characters.emplace_back(result.getInt64Field("id"), result.getStringField("name"));
		result.nextRow();
	}
	return characters;
}

nlohmann::json SQLiteDatabase::GetPendingNamesTable(const uint32_t start, const uint32_t length) {
	auto [_, countResult] = ExecuteSelect("SELECT COUNT(*) AS count FROM charinfo WHERE pending_name != '' AND needs_rename = 0;");
	const uint32_t total = countResult.eof() ? 0 : countResult.getIntField("count");

	auto [__, result] = ExecuteSelect(
		"SELECT c.id, c.name, c.pending_name, c.account_id, a.name AS account_name "
		"FROM charinfo c LEFT JOIN accounts a ON a.id = c.account_id "
		"WHERE c.pending_name != '' AND c.needs_rename = 0 ORDER BY c.id LIMIT ? OFFSET ?;", length, start);

	nlohmann::json data = nlohmann::json::array();
	while (!result.eof()) {
		data.push_back({
			{"id", std::to_string(result.getInt64Field("id"))},
			{"name", result.getStringField("name")},
			{"pending_name", result.getStringField("pending_name")},
			{"account_id", result.getIntField("account_id")},
			{"account_name", NullableString(result, "account_name")}
		});
		result.nextRow();
	}
	return nlohmann::json{ {"draw", 0}, {"recordsTotal", total}, {"recordsFiltered", total}, {"data", data} };
}

void SQLiteDatabase::SetCharacterPermissionMap(const LWOOBJID characterId, const uint64_t permissionMap) {
	ExecuteUpdate("UPDATE charinfo SET permission_map = ? WHERE id = ?;", permissionMap, characterId);
}

IDashboardStats::Snapshot SQLiteDatabase::GetDashboardSnapshot() {
	IDashboardStats::Snapshot snapshot;
	auto [_, result] = ExecuteSelect(SNAPSHOT_QUERY);
	if (result.eof()) return snapshot;
	snapshot.accounts = result.getInt64Field("accounts");
	snapshot.accountsMaxId = result.getInt64Field("accounts_max_id");
	snapshot.characters = result.getInt64Field("characters");
	snapshot.pendingNames = result.getInt64Field("pending_names");
	snapshot.properties = result.getInt64Field("properties");
	snapshot.pendingProperties = result.getInt64Field("pending_properties");
	snapshot.playKeys = result.getInt64Field("play_keys");
	snapshot.bugReports = result.getInt64Field("bug_reports");
	snapshot.unresolvedBugReports = result.getInt64Field("unresolved_bug_reports");
	snapshot.petNames = result.getInt64Field("pet_names");
	snapshot.pendingPetNames = result.getInt64Field("pending_pet_names");
	snapshot.activityLogMaxId = result.getInt64Field("activity_log_max_id");
	snapshot.chatLogMaxId = result.getInt64Field("chat_log_max_id");
	snapshot.commandLogMaxId = result.getInt64Field("command_log_max_id");
	snapshot.auditLogMaxId = result.getInt64Field("audit_log_max_id");
	snapshot.mailMaxId = result.getInt64Field("mail_max_id");
	snapshot.openEconomyFlags = result.getInt64Field("open_economy_flags");
	snapshot.economyFlagsMaxId = result.getInt64Field("economy_flags_max_id");
	return snapshot;
}

std::optional<LWOOBJID> SQLiteDatabase::GetModelPropertyId(const LWOOBJID modelID) {
	auto [_, result] = ExecuteSelect("SELECT property_id FROM properties_contents WHERE id = ? LIMIT 1;", modelID);
	if (result.eof()) return std::nullopt;
	return result.getInt64Field("property_id");
}

std::optional<int32_t> SQLiteDatabase::GetRedeemablePlayKeyId(const std::string_view keyString) {
	auto [_, result] = ExecuteSelect(
		"SELECT p.id FROM play_keys p WHERE p.key_string = ? AND p.active = 1 "
		"AND (SELECT COUNT(*) FROM accounts a WHERE a.play_key_id = p.id) < p.key_uses LIMIT 1;", keyString);
	if (result.eof()) return std::nullopt;
	return result.getIntField("id");
}

void SQLiteDatabase::SetAccountPlayKey(const uint32_t accountId, const int32_t playkeyId) {
	ExecuteUpdate("UPDATE accounts SET play_key_id = ? WHERE id = ?;", playkeyId, accountId);
}

std::optional<IAccountEmails::EmailInfo> SQLiteDatabase::GetAccountEmail(const uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT email, email_confirmed_at FROM accounts WHERE id = ? LIMIT 1;", accountId);
	if (result.eof()) return std::nullopt;
	return IAccountEmails::EmailInfo{ result.fieldIsNull("email") ? "" : result.getStringField("email"), !result.fieldIsNull("email_confirmed_at") };
}

void SQLiteDatabase::SetAccountEmail(const uint32_t accountId, const std::string_view email, const bool confirmed) {
	if (confirmed) ExecuteUpdate("UPDATE accounts SET email = ?, email_confirmed_at = CURRENT_TIMESTAMP WHERE id = ?;", email, accountId);
	else ExecuteUpdate("UPDATE accounts SET email = ?, email_confirmed_at = NULL WHERE id = ?;", email, accountId);
}

std::optional<uint32_t> SQLiteDatabase::GetAccountIdByConfirmedEmail(const std::string_view email) {
	auto [_, result] = ExecuteSelect("SELECT id FROM accounts WHERE LOWER(email) = LOWER(?) AND email_confirmed_at IS NOT NULL LIMIT 1;", email);
	if (result.eof()) return std::nullopt;
	return static_cast<uint32_t>(result.getIntField("id"));
}

void SQLiteDatabase::InsertAccountToken(const std::string_view tokenHash, const uint32_t accountId, const std::string_view purpose, const std::string_view data, const int64_t expiresAt) {
	ExecuteInsert("INSERT INTO account_tokens (token_hash, account_id, purpose, data, expires_at) VALUES (?, ?, ?, ?, ?);", tokenHash, accountId, purpose, data, expiresAt);
}

std::optional<IAccountEmails::AccountToken> SQLiteDatabase::ConsumeAccountToken(const std::string_view tokenHash, const std::string_view purpose) {
	std::optional<IAccountEmails::AccountToken> token;
	{
		auto [_, result] = ExecuteSelect("SELECT account_id, data FROM account_tokens WHERE token_hash = ? AND purpose = ? AND expires_at > ? LIMIT 1;",
			tokenHash, purpose, static_cast<int64_t>(std::time(nullptr)));
		if (!result.eof()) token = IAccountEmails::AccountToken{ static_cast<uint32_t>(result.getIntField("account_id")), result.getStringField("data") };
	}
	ExecuteDelete("DELETE FROM account_tokens WHERE token_hash = ?;", tokenHash);
	return token;
}

void SQLiteDatabase::DeleteAccountTokens(const uint32_t accountId, const std::string_view purpose) {
	ExecuteDelete("DELETE FROM account_tokens WHERE account_id = ? AND purpose = ?;", accountId, purpose);
}

void SQLiteDatabase::DeleteExpiredAccountTokens() {
	ExecuteDelete("DELETE FROM account_tokens WHERE expires_at <= ?;", static_cast<int64_t>(std::time(nullptr)));
}

int64_t SQLiteDatabase::GetSessionsValidAfter(const uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT sessions_valid_after FROM accounts WHERE id = ? LIMIT 1;", accountId);
	return result.eof() ? 0 : result.getInt64Field("sessions_valid_after");
}

void SQLiteDatabase::SetSessionsValidAfter(const uint32_t accountId, const int64_t time) {
	ExecuteUpdate("UPDATE accounts SET sessions_valid_after = ? WHERE id = ?;", time, accountId);
}

uint32_t SQLiteDatabase::ApprovePreviouslyApprovedPetNames() {
	// The derived table keeps this valid on MySQL, which can't select from the table being updated
	return static_cast<uint32_t>(ExecuteUpdate(
		"UPDATE pet_names SET approved = 2 WHERE approved = 1 AND pet_name IN "
		"(SELECT name FROM (SELECT pet_name AS name FROM pet_names WHERE approved = 2) AS approved_names);"));
}

std::vector<std::pair<LWOOBJID, std::string>> SQLiteDatabase::GetAllPetNames() {
	std::vector<std::pair<LWOOBJID, std::string>> pets;
	auto [_, result] = ExecuteSelect("SELECT id, pet_name FROM pet_names;");
	while (!result.eof()) {
		pets.emplace_back(result.getInt64Field("id"), result.getStringField("pet_name"));
		result.nextRow();
	}
	return pets;
}

void SQLiteDatabase::ForEachCharacterXml(const std::function<void(LWOOBJID, const std::string&)>& visit) {
	auto [_, result] = ExecuteSelect("SELECT id, xml_data FROM charxml;");
	while (!result.eof()) {
		visit(result.getInt64Field("id"), result.getStringField("xml_data"));
		result.nextRow();
	}
}

void SQLiteDatabase::ForEachCharacterXmlContaining(const std::string& needle, const std::function<void(LWOOBJID, const std::string&)>& visit) {
	auto [_, result] = ExecuteSelect("SELECT id, xml_data FROM charxml WHERE instr(xml_data, ?) > 0;", needle);
	while (!result.eof()) {
		visit(result.getInt64Field("id"), result.getStringField("xml_data"));
		result.nextRow();
	}
}

uint32_t SQLiteDatabase::FixPropertyCloneIds() {
	return static_cast<uint32_t>(ExecuteUpdate(
		"UPDATE properties SET clone_id = (SELECT c.prop_clone_id FROM charinfo c WHERE c.id = properties.owner_id) "
		"WHERE EXISTS (SELECT 1 FROM charinfo c WHERE c.id = properties.owner_id AND c.prop_clone_id IS NOT NULL "
		"AND (properties.clone_id IS NULL OR c.prop_clone_id != properties.clone_id));"));
}
