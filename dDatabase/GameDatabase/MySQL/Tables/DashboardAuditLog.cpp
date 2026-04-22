#include "MySQLDatabase.h"
#include <ctime>

void MySQLDatabase::InsertAuditLog(const std::string_view ip, const std::string_view endpoint,
    const std::string_view method, const std::string_view user_agent, int32_t response_code) {
    uint64_t now = static_cast<uint64_t>(std::time(nullptr));
    ExecuteInsert("INSERT INTO dashboard_audit_log (timestamp, ip_address, endpoint, method, user_agent, response_code) VALUES (?, ?, ?, ?, ?, ?);",
        now, ip, endpoint, method, user_agent, response_code);
}

std::vector<IDashboardAuditLog::AuditLogEntry> MySQLDatabase::GetRecentAuditLogs(uint32_t limit) {
    std::vector<IDashboardAuditLog::AuditLogEntry> logs;
    auto res = ExecuteSelect("SELECT id, timestamp, ip_address, endpoint, method, user_agent, response_code FROM dashboard_audit_log ORDER BY timestamp DESC LIMIT ?;", limit);

    while (res->next()) {
        IDashboardAuditLog::AuditLogEntry entry;
        entry.id = res->getUInt64("id");
        entry.timestamp = res->getUInt64("timestamp");
        entry.ip_address = res->getString("ip_address").c_str();
        entry.endpoint = res->getString("endpoint").c_str();
        entry.method = res->getString("method").c_str();
        entry.user_agent = res->getString("user_agent").c_str();
        entry.response_code = res->getInt("response_code");
        logs.push_back(entry);
    }

    return logs;
}

std::vector<IDashboardAuditLog::AuditLogEntry> MySQLDatabase::GetAuditLogsByIP(const std::string_view ip, uint32_t limit) {
    std::vector<IDashboardAuditLog::AuditLogEntry> logs;
    auto res = ExecuteSelect("SELECT id, timestamp, ip_address, endpoint, method, user_agent, response_code FROM dashboard_audit_log WHERE ip_address = ? ORDER BY timestamp DESC LIMIT ?;", ip, limit);

    while (res->next()) {
        IDashboardAuditLog::AuditLogEntry entry;
        entry.id = res->getUInt64("id");
        entry.timestamp = res->getUInt64("timestamp");
        entry.ip_address = res->getString("ip_address").c_str();
        entry.endpoint = res->getString("endpoint").c_str();
        entry.method = res->getString("method").c_str();
        entry.user_agent = res->getString("user_agent").c_str();
        entry.response_code = res->getInt("response_code");
        logs.push_back(entry);
    }

    return logs;
}

void MySQLDatabase::CleanupOldAuditLogs(uint32_t days_to_keep) {
    uint64_t cutoff_time = static_cast<uint64_t>(std::time(nullptr)) - (static_cast<uint64_t>(days_to_keep) * 86400ULL);
    ExecuteDelete("DELETE FROM dashboard_audit_log WHERE timestamp < ?;", cutoff_time);
}

void MySQLDatabase::InsertAdminActionLog(uint32_t adminAccountId, const std::string_view action,
                                          const std::string_view targetType, uint64_t targetId,
                                          const std::string_view details) {
    // dashboard_admin_action_log table doesn't exist
    // Admin actions could be logged to dashboard_audit_log or a new table needs to be created
    // For now, this is a no-op
}

std::vector<IDashboardAuditLog::AdminActionLog> MySQLDatabase::GetAuditLogs(uint32_t limit) {
    // dashboard_admin_action_log table doesn't exist
    // Would need to create a new table or use dashboard_audit_log with additional columns
    return {};
}
