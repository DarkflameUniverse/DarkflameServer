#include "MySQLDatabase.h"

#include <ctime>

void MySQLDatabase::ReportConfigFromFile(const Setting& setting) {
	ExecuteInsert(
		"INSERT INTO server_config (file, name, file_value, file_source, secret, description, seen_at) VALUES (?, ?, ?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE file_value = VALUES(file_value), file_source = VALUES(file_source), secret = VALUES(secret), "
		"description = IF(VALUES(description) = '', description, VALUES(description)), seen_at = VALUES(seen_at);",
		setting.file, setting.name, setting.fileValue ? std::optional<std::string>(*setting.fileValue) : std::nullopt, setting.fileSource,
		setting.secret, setting.description, static_cast<int64_t>(std::time(nullptr)));
}

std::vector<IServerConfig::Setting> MySQLDatabase::GetServerConfig(const std::vector<std::string>& files) {
	std::vector<Setting> settings;
	auto result = ExecuteSelect("SELECT * FROM server_config ORDER BY file, name;");
	const auto text = [&](const char* field) { return std::string(result->getString(field).c_str()); };
	while (result->next()) {
		Setting setting;
		setting.file = text("file");
		if (!files.empty() && std::find(files.begin(), files.end(), setting.file) == files.end()) continue;
		setting.name = text("name");
		if (!result->isNull("file_value")) setting.fileValue = text("file_value");
		setting.fileSource = text("file_source");
		if (!result->isNull("web_value")) setting.webValue = text("web_value");
		setting.webWins = result->getInt("web_wins") != 0;
		setting.secret = result->getInt("secret") != 0;
		setting.description = result->isNull("description") ? "" : text("description");
		setting.seenAt = result->getInt64("seen_at");
		setting.updatedAt = result->getInt64("updated_at");
		setting.updatedBy = text("updated_by");
		settings.push_back(std::move(setting));
	}
	return settings;
}

void MySQLDatabase::SetWebConfigValue(const std::string& file, const std::string& name, const std::optional<std::string>& value, bool webWins, const std::string& by) {
	ExecuteInsert(
		"INSERT INTO server_config (file, name, web_value, web_wins, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE web_value = VALUES(web_value), web_wins = VALUES(web_wins), updated_at = VALUES(updated_at), updated_by = VALUES(updated_by);",
		file, name, value, webWins, static_cast<int64_t>(std::time(nullptr)), by);
}

void MySQLDatabase::DeleteServerConfig(const std::string& file, const std::string& name) {
	ExecuteDelete("DELETE FROM server_config WHERE file = ? AND name = ?;", file, name);
}

namespace {
	IServerConfig::SettingChange ReadSettingChange(PreparedStmtResultSet& result) {
		const auto text = [&](const char* field) { return std::string(result->getString(field).c_str()); };
		const auto optional = [&](const char* field) { return result->isNull(field) ? std::nullopt : std::optional<std::string>(text(field)); };
		IServerConfig::SettingChange change;
		change.id = result->getUInt64("id");
		change.file = text("file");
		change.name = text("name");
		change.oldValue = optional("old_value");
		change.oldWebWins = result->getInt("old_web_wins") != 0;
		change.newValue = optional("new_value");
		change.newWebWins = result->getInt("new_web_wins") != 0;
		change.fileValue = optional("file_value");
		change.secret = result->getInt("secret") != 0;
		change.removed = result->getInt("removed") != 0;
		change.revertOf = result->getUInt64("revert_of");
		change.changedAt = result->getInt64("changed_at");
		change.accountId = result->getUInt("account_id");
		change.changedBy = text("changed_by");
		return change;
	}
}

uint64_t MySQLDatabase::InsertSettingChange(const SettingChange& change) {
	ExecuteInsert(
		"INSERT INTO server_config_history (file, name, old_value, old_web_wins, new_value, new_web_wins, file_value, secret, removed, revert_of, changed_at, account_id, changed_by) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		change.file, change.name, change.oldValue, change.oldWebWins, change.newValue, change.newWebWins, change.fileValue, change.secret, change.removed,
		change.revertOf, change.changedAt, change.accountId, change.changedBy);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;"); // this connection's insert
	return result->next() ? result->getUInt64("id") : 0;
}

std::vector<IServerConfig::SettingChange> MySQLDatabase::GetSettingChanges(const std::string& file, const std::string& name, uint32_t start, uint32_t length, uint64_t& total) {
	std::vector<SettingChange> changes;
	const bool all = file.empty() && name.empty();
	auto count = all ? ExecuteSelect("SELECT COUNT(*) AS count FROM server_config_history;")
		: ExecuteSelect("SELECT COUNT(*) AS count FROM server_config_history WHERE file = ? AND name = ?;", file, name);
	total = count->next() ? count->getUInt64("count") : 0;
	auto result = all ? ExecuteSelect("SELECT * FROM server_config_history ORDER BY id DESC LIMIT ? OFFSET ?;", length, start)
		: ExecuteSelect("SELECT * FROM server_config_history WHERE file = ? AND name = ? ORDER BY id DESC LIMIT ? OFFSET ?;", file, name, length, start);
	while (result->next()) changes.push_back(ReadSettingChange(result));
	return changes;
}

std::optional<IServerConfig::SettingChange> MySQLDatabase::GetSettingChange(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM server_config_history WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadSettingChange(result);
}
