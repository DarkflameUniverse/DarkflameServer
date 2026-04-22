#include "MySQLDatabase.h"

std::optional<std::string> MySQLDatabase::GetDashboardConfig(const std::string_view key) {
    auto res = ExecuteSelect("SELECT config_value FROM dashboard_config WHERE config_key = ? LIMIT 1;", key);
    if (res->next()) {
        return res->getString("config_value").c_str();
    }
    return std::nullopt;
}

void MySQLDatabase::SetDashboardConfig(const std::string_view key, const std::string_view value) {
    // Use INSERT ... ON DUPLICATE KEY UPDATE for MySQL
    ExecuteInsert("INSERT INTO dashboard_config (config_key, config_value, updated_at) VALUES (?, ?, UNIX_TIMESTAMP()) ON DUPLICATE KEY UPDATE config_value = VALUES(config_value), updated_at = VALUES(updated_at);", key, value);
}
