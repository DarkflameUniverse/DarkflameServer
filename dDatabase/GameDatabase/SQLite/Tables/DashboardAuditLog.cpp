#include "SQLiteDatabase.h"
#include <ctime>

void SQLiteDatabase::InsertAuditLog(const std::string_view ip, const std::string_view endpoint,
                                     const std::string_view method, const std::string_view user_agent,
                                     int32_t response_code) {
	uint64_t now = static_cast<uint64_t>(std::time(nullptr));
	ExecuteInsert("INSERT INTO dashboard_audit_log (timestamp, ip_address, endpoint, method, user_agent, response_code) VALUES (?, ?, ?, ?, ?, ?);",
	              now, ip, endpoint, method, user_agent, response_code);
}

std::vector<IDashboardAuditLog::AuditLogEntry> SQLiteDatabase::GetRecentAuditLogs(uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT id, timestamp, ip_address, endpoint, method, user_agent, response_code FROM dashboard_audit_log ORDER BY timestamp DESC LIMIT ?;", limit);
	
	std::vector<IDashboardAuditLog::AuditLogEntry> logs;
	while (result.nextRow()) {
		IDashboardAuditLog::AuditLogEntry entry;
		entry.id = result.getInt64Field(0);
		entry.timestamp = result.getInt64Field(1);
		entry.ip_address = result.getStringField(2);
		entry.endpoint = result.getStringField(3);
		entry.method = result.getStringField(4);
		entry.user_agent = result.getStringField(5);
		entry.response_code = result.getIntField(6);
		logs.push_back(entry);
	}
	
	return logs;
}

std::vector<IDashboardAuditLog::AuditLogEntry> SQLiteDatabase::GetAuditLogsByIP(const std::string_view ip, uint32_t limit) {
	auto [_, result] = ExecuteSelect("SELECT id, timestamp, ip_address, endpoint, method, user_agent, response_code FROM dashboard_audit_log WHERE ip_address = ? ORDER BY timestamp DESC LIMIT ?;", ip, limit);
	
	std::vector<IDashboardAuditLog::AuditLogEntry> logs;
	while (result.nextRow()) {
		IDashboardAuditLog::AuditLogEntry entry;
		entry.id = result.getInt64Field(0);
		entry.timestamp = result.getInt64Field(1);
		entry.ip_address = result.getStringField(2);
		entry.endpoint = result.getStringField(3);
		entry.method = result.getStringField(4);
		entry.user_agent = result.getStringField(5);
		entry.response_code = result.getIntField(6);
		logs.push_back(entry);
	}
	
	return logs;
}

void SQLiteDatabase::CleanupOldAuditLogs(uint32_t days_to_keep) {
	uint64_t cutoff_time = static_cast<uint64_t>(std::time(nullptr)) - (days_to_keep * 86400);
	ExecuteDelete("DELETE FROM dashboard_audit_log WHERE timestamp < ?;", cutoff_time);
}

void SQLiteDatabase::InsertAdminActionLog(uint32_t adminAccountId, const std::string_view action,
                                           const std::string_view targetType, uint64_t targetId,
                                           const std::string_view details) {
	// dashboard_admin_action_log table doesn't exist
	// Admin actions could be logged to dashboard_audit_log or a new table needs to be created
	// For now, this is a no-op
}

std::vector<IDashboardAuditLog::AdminActionLog> SQLiteDatabase::GetAuditLogs(uint32_t limit) {
	// dashboard_admin_action_log table doesn't exist
	// Would need to create a new table or use dashboard_audit_log with additional columns
	return {};
}
