#ifndef __IDASHBOARDAUDITLOG__H__
#define __IDASHBOARDAUDITLOG__H__

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/**
 * Interface for Dashboard audit log table.
 * Records all API requests, security events, and administrative actions.
 */
class IDashboardAuditLog {
public:

	struct AuditLogEntry {
		uint64_t id;
		uint64_t timestamp;
		std::string ip_address;
		std::string endpoint;
		std::string method;
		std::string user_agent;
		int32_t response_code;
	};

	struct AdminActionLog {
		uint64_t timestamp;
		uint32_t adminAccountId;
		std::string action;
		std::string targetType;
		uint64_t targetId;
		std::string details;
	};

	// Insert a new audit log entry for API requests
	virtual void InsertAuditLog(const std::string_view ip, const std::string_view endpoint,
	                             const std::string_view method, const std::string_view user_agent,
	                             int32_t response_code) = 0;

	// Insert a new admin action log entry
	virtual void InsertAdminActionLog(uint32_t adminAccountId, const std::string_view action,
	                                    const std::string_view targetType, uint64_t targetId,
	                                    const std::string_view details) = 0;

	// Get recent audit log entries (limit = number of entries)
	virtual std::vector<AuditLogEntry> GetRecentAuditLogs(uint32_t limit = 100) = 0;

	// Get recent admin action logs
	virtual std::vector<AdminActionLog> GetAuditLogs(uint32_t limit = 100) = 0;

	// Get audit logs for a specific IP address
	virtual std::vector<AuditLogEntry> GetAuditLogsByIP(const std::string_view ip, uint32_t limit = 100) = 0;

	// Clear old audit logs (older than days_to_keep)
	virtual void CleanupOldAuditLogs(uint32_t days_to_keep = 30) = 0;
};

#endif  //!__IDASHBOARDAUDITLOG__H__
