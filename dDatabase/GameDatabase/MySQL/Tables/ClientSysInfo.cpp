#include "MySQLDatabase.h"

#include "ClientSysInfoSql.h"
#include "GeneralUtils.h"

namespace {
	IClientSysInfo::SysInfoRow Row(sql::ResultSet& r, const bool withName = false) {
		IClientSysInfo::SysInfoRow s;
		s.id = r.getUInt64("id");
		s.accountId = r.getUInt("account_id");
		s.firstSeen = r.getInt64("first_seen");
		s.lastSeen = r.getInt64("last_seen");
		s.logins = r.getUInt("logins");
		s.ip = r.getString("ip").c_str();
		s.clientOs = r.getUInt("client_os");
		s.memoryStats = r.getString("memory_stats").c_str();
		s.memoryTotalKb = r.getUInt64("memory_total_kb");
		s.videoCard = r.getString("video_card").c_str();
		s.numberOfProcessors = r.getUInt("number_of_processors");
		s.processorType = r.getUInt("processor_type");
		s.processorLevel = static_cast<uint16_t>(r.getUInt("processor_level"));
		s.processorRevision = static_cast<uint16_t>(r.getUInt("processor_revision"));
		s.osVersionInfoSize = r.getUInt("os_version_info_size");
		s.majorVersion = r.getUInt("os_major_version");
		s.minorVersion = r.getUInt("os_minor_version");
		s.buildNumber = r.getUInt("os_build_number");
		s.platformId = r.getUInt("os_platform_id");
		if (withName) s.accountName = r.getString("account_name").c_str();
		return s;
	}
}

void MySQLDatabase::RecordClientSysInfo(const SysInfoRow& s) {
	auto newest = ExecuteSelect("SELECT * FROM client_sysinfo WHERE account_id = ? ORDER BY id DESC LIMIT 1;", s.accountId);
	if (newest->next()) {
		const auto previous = Row(*newest.m_resultSet);
		if (previous.SameAs(s)) {
			ExecuteUpdate("UPDATE client_sysinfo SET last_seen = ?, logins = logins + 1, memory_stats = ? WHERE id = ?;", s.lastSeen, s.memoryStats, previous.id);
			return;
		}
	}
	ExecuteInsert("INSERT INTO client_sysinfo (account_id, first_seen, last_seen, logins, ip, client_os, memory_stats, memory_total_kb, video_card, "
		"number_of_processors, processor_type, processor_level, processor_revision, os_version_info_size, os_major_version, os_minor_version, "
		"os_build_number, os_platform_id) VALUES (?, ?, ?, 1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		s.accountId, s.firstSeen, s.lastSeen, s.ip, s.clientOs, s.memoryStats, s.memoryTotalKb, s.videoCard, s.numberOfProcessors, s.processorType,
		s.processorLevel, s.processorRevision, s.osVersionInfoSize, s.majorVersion, s.minorVersion, s.buildNumber, s.platformId);
}

std::vector<IClientSysInfo::SysInfoRow> MySQLDatabase::GetClientSysInfo(const uint32_t accountId, const uint32_t limit) {
	std::vector<SysInfoRow> rows;
	auto result = ExecuteSelect("SELECT * FROM client_sysinfo WHERE account_id = ? ORDER BY id DESC LIMIT ?;", accountId, limit);
	while (result->next()) rows.push_back(Row(*result.m_resultSet));
	return rows;
}

std::vector<IClientSysInfo::SysInfoRow> MySQLDatabase::GetLatestClientSysInfo(const uint32_t limit) {
	std::vector<SysInfoRow> rows;
	auto result = ExecuteSelect("SELECT c.* FROM client_sysinfo c JOIN (SELECT MAX(id) AS id FROM client_sysinfo GROUP BY account_id) n ON n.id = c.id "
		"ORDER BY c.id DESC LIMIT ?;", limit);
	while (result->next()) rows.push_back(Row(*result.m_resultSet));
	return rows;
}

std::vector<IClientSysInfo::SysInfoRow> MySQLDatabase::ListClientSysInfo(const SysInfoQuery& q) {
	const auto pattern = GeneralUtils::LikeEscape(q.search, '!');
	auto result = ExecuteSelect("SELECT c.*, COALESCE(a.name, '') AS account_name" + ClientSysInfoSql::From(q, "CONCAT('%', ?, '%')") +
		ClientSysInfoSql::Order(q) + " LIMIT ? OFFSET ?;", q.accountId, q.accountId, q.search, pattern, pattern, q.limit, q.offset);
	std::vector<SysInfoRow> rows;
	while (result->next()) rows.push_back(Row(*result.m_resultSet, true));
	return rows;
}

uint64_t MySQLDatabase::CountClientSysInfo(const SysInfoQuery& q) {
	const auto pattern = GeneralUtils::LikeEscape(q.search, '!');
	auto result = ExecuteSelect("SELECT COUNT(*) AS count" + ClientSysInfoSql::From(q, "CONCAT('%', ?, '%')") + ";",
		q.accountId, q.accountId, q.search, pattern, pattern);
	return result->next() ? result->getUInt64("count") : 0;
}
