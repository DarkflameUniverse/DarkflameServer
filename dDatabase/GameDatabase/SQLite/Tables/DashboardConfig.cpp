#include "SQLiteDatabase.h"

std::optional<std::string> SQLiteDatabase::GetDashboardConfig(const std::string_view key) {
	auto [_, result] = ExecuteSelect("SELECT config_value FROM dashboard_config WHERE config_key = ?;", key);
	
	if (result.nextRow()) {
		return result.getStringField(0);
	}
	
	return std::nullopt;
}

void SQLiteDatabase::SetDashboardConfig(const std::string_view key, const std::string_view value) {
	ExecuteInsert("INSERT OR REPLACE INTO dashboard_config (config_key, config_value, updated_at) VALUES (?, ?, strftime('%s', 'now'));", key, value);
}
