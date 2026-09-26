#include "SQLiteDatabase.h"

#include <ctime>

void SQLiteDatabase::ReportConfigFromFile(const Setting& setting) {
	ExecuteInsert(
		"INSERT INTO server_config (file, name, file_value, file_source, secret, description, seen_at) VALUES (?, ?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(file, name) DO UPDATE SET file_value = excluded.file_value, file_source = excluded.file_source, secret = excluded.secret, "
		"description = CASE WHEN excluded.description = '' THEN server_config.description ELSE excluded.description END, seen_at = excluded.seen_at;",
		setting.file, setting.name, setting.fileValue ? std::optional<std::string>(*setting.fileValue) : std::nullopt, setting.fileSource,
		setting.secret, setting.description, static_cast<int64_t>(std::time(nullptr)));
}

std::vector<IServerConfig::Setting> SQLiteDatabase::GetServerConfig(const std::vector<std::string>& files) {
	std::vector<Setting> settings;
	auto [_, result] = ExecuteSelect("SELECT * FROM server_config ORDER BY file, name;");
	while (!result.eof()) {
		Setting setting;
		setting.file = result.getStringField("file");
		if (files.empty() || std::find(files.begin(), files.end(), setting.file) != files.end()) {
			setting.name = result.getStringField("name");
			if (!result.fieldIsNull("file_value")) setting.fileValue = result.getStringField("file_value");
			setting.fileSource = result.getStringField("file_source");
			if (!result.fieldIsNull("web_value")) setting.webValue = result.getStringField("web_value");
			setting.webWins = result.getIntField("web_wins") != 0;
			setting.secret = result.getIntField("secret") != 0;
			setting.description = result.fieldIsNull("description") ? "" : result.getStringField("description");
			setting.seenAt = result.getInt64Field("seen_at");
			setting.updatedAt = result.getInt64Field("updated_at");
			setting.updatedBy = result.getStringField("updated_by");
			settings.push_back(std::move(setting));
		}
		result.nextRow();
	}
	return settings;
}

void SQLiteDatabase::SetWebConfigValue(const std::string& file, const std::string& name, const std::optional<std::string>& value, bool webWins, const std::string& by) {
	ExecuteInsert(
		"INSERT INTO server_config (file, name, web_value, web_wins, updated_at, updated_by) VALUES (?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(file, name) DO UPDATE SET web_value = excluded.web_value, web_wins = excluded.web_wins, updated_at = excluded.updated_at, updated_by = excluded.updated_by;",
		file, name, value, webWins, static_cast<int64_t>(std::time(nullptr)), by);
}

void SQLiteDatabase::DeleteServerConfig(const std::string& file, const std::string& name) {
	ExecuteDelete("DELETE FROM server_config WHERE file = ? AND name = ?;", file, name);
}

namespace {
	std::optional<std::string> OptionalText(CppSQLite3Query& result, const char* field) {
		if (result.fieldIsNull(field)) return std::nullopt;
		return std::string(result.getStringField(field));
	}

	IServerConfig::SettingChange ReadSettingChange(CppSQLite3Query& result) {
		IServerConfig::SettingChange change;
		change.id = static_cast<uint64_t>(result.getInt64Field("id"));
		change.file = result.getStringField("file");
		change.name = result.getStringField("name");
		change.oldValue = OptionalText(result, "old_value");
		change.oldWebWins = result.getIntField("old_web_wins") != 0;
		change.newValue = OptionalText(result, "new_value");
		change.newWebWins = result.getIntField("new_web_wins") != 0;
		change.fileValue = OptionalText(result, "file_value");
		change.secret = result.getIntField("secret") != 0;
		change.removed = result.getIntField("removed") != 0;
		change.revertOf = static_cast<uint64_t>(result.getInt64Field("revert_of"));
		change.changedAt = result.getInt64Field("changed_at");
		change.accountId = static_cast<uint32_t>(result.getIntField("account_id"));
		change.changedBy = result.getStringField("changed_by");
		return change;
	}
}

uint64_t SQLiteDatabase::InsertSettingChange(const SettingChange& change) {
	ExecuteInsert(
		"INSERT INTO server_config_history (file, name, old_value, old_web_wins, new_value, new_web_wins, file_value, secret, removed, revert_of, changed_at, account_id, changed_by) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		change.file, change.name, change.oldValue, change.oldWebWins, change.newValue, change.newWebWins, change.fileValue, change.secret, change.removed,
		static_cast<int64_t>(change.revertOf), change.changedAt, change.accountId, change.changedBy);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

std::vector<IServerConfig::SettingChange> SQLiteDatabase::GetSettingChanges(const std::string& file, const std::string& name, uint32_t start, uint32_t length, uint64_t& total) {
	std::vector<SettingChange> changes;
	const bool all = file.empty() && name.empty();
	{
		auto [_, count] = all ? ExecuteSelect("SELECT COUNT(*) AS count FROM server_config_history;")
			: ExecuteSelect("SELECT COUNT(*) AS count FROM server_config_history WHERE file = ? AND name = ?;", file, name);
		total = count.eof() ? 0 : static_cast<uint64_t>(count.getInt64Field("count"));
	}
	auto [_, result] = all ? ExecuteSelect("SELECT * FROM server_config_history ORDER BY id DESC LIMIT ? OFFSET ?;", length, start)
		: ExecuteSelect("SELECT * FROM server_config_history WHERE file = ? AND name = ? ORDER BY id DESC LIMIT ? OFFSET ?;", file, name, length, start);
	for (; !result.eof(); result.nextRow()) changes.push_back(ReadSettingChange(result));
	return changes;
}

std::optional<IServerConfig::SettingChange> SQLiteDatabase::GetSettingChange(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM server_config_history WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return ReadSettingChange(result);
}
