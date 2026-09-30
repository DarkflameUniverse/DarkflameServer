#include "SQLiteDatabase.h"

#include "ClientSysInfoSql.h"
#include "GeneralUtils.h"

namespace {
	IClientSysInfo::SysInfoRow Row(CppSQLite3Query& r, const bool withName = false) {
		IClientSysInfo::SysInfoRow s;
		s.id = static_cast<uint64_t>(r.getInt64Field("id"));
		s.accountId = static_cast<uint32_t>(r.getInt64Field("account_id"));
		s.firstSeen = r.getInt64Field("first_seen");
		s.lastSeen = r.getInt64Field("last_seen");
		s.logins = static_cast<uint32_t>(r.getInt64Field("logins"));
		s.ip = r.getStringField("ip");
		s.clientOs = static_cast<uint32_t>(r.getInt64Field("client_os"));
		s.memoryStats = r.getStringField("memory_stats");
		s.memoryTotalKb = static_cast<uint64_t>(r.getInt64Field("memory_total_kb"));
		s.videoCard = r.getStringField("video_card");
		s.numberOfProcessors = static_cast<uint32_t>(r.getInt64Field("number_of_processors"));
		s.processorType = static_cast<uint32_t>(r.getInt64Field("processor_type"));
		s.processorLevel = static_cast<uint16_t>(r.getInt64Field("processor_level"));
		s.processorRevision = static_cast<uint16_t>(r.getInt64Field("processor_revision"));
		s.osVersionInfoSize = static_cast<uint32_t>(r.getInt64Field("os_version_info_size"));
		s.majorVersion = static_cast<uint32_t>(r.getInt64Field("os_major_version"));
		s.minorVersion = static_cast<uint32_t>(r.getInt64Field("os_minor_version"));
		s.buildNumber = static_cast<uint32_t>(r.getInt64Field("os_build_number"));
		s.platformId = static_cast<uint32_t>(r.getInt64Field("os_platform_id"));
		if (withName) s.accountName = r.getStringField("account_name");
		return s;
	}
}

void SQLiteDatabase::RecordClientSysInfo(const SysInfoRow& s) {
	{
		auto [_, newest] = ExecuteSelect("SELECT * FROM client_sysinfo WHERE account_id = ? ORDER BY id DESC LIMIT 1;", s.accountId);
		if (!newest.eof()) {
			const auto previous = Row(newest);
			if (previous.SameAs(s)) {
				ExecuteUpdate("UPDATE client_sysinfo SET last_seen = ?, logins = logins + 1, memory_stats = ? WHERE id = ?;", s.lastSeen, s.memoryStats, previous.id);
				return;
			}
		}
	}
	ExecuteInsert("INSERT INTO client_sysinfo (account_id, first_seen, last_seen, logins, ip, client_os, memory_stats, memory_total_kb, video_card, "
		"number_of_processors, processor_type, processor_level, processor_revision, os_version_info_size, os_major_version, os_minor_version, "
		"os_build_number, os_platform_id) VALUES (?, ?, ?, 1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		s.accountId, s.firstSeen, s.lastSeen, s.ip, s.clientOs, s.memoryStats, s.memoryTotalKb, s.videoCard, s.numberOfProcessors, s.processorType,
		s.processorLevel, s.processorRevision, s.osVersionInfoSize, s.majorVersion, s.minorVersion, s.buildNumber, s.platformId);
}

std::vector<IClientSysInfo::SysInfoRow> SQLiteDatabase::GetClientSysInfo(const uint32_t accountId, const uint32_t limit) {
	std::vector<SysInfoRow> rows;
	auto [_, result] = ExecuteSelect("SELECT * FROM client_sysinfo WHERE account_id = ? ORDER BY id DESC LIMIT ?;", accountId, limit);
	for (; !result.eof(); result.nextRow()) rows.push_back(Row(result));
	return rows;
}

std::vector<IClientSysInfo::SysInfoRow> SQLiteDatabase::GetLatestClientSysInfo(const uint32_t limit) {
	std::vector<SysInfoRow> rows;
	auto [_, result] = ExecuteSelect("SELECT c.* FROM client_sysinfo c JOIN (SELECT MAX(id) AS id FROM client_sysinfo GROUP BY account_id) n ON n.id = c.id "
		"ORDER BY c.id DESC LIMIT ?;", limit);
	for (; !result.eof(); result.nextRow()) rows.push_back(Row(result));
	return rows;
}

std::vector<IClientSysInfo::SysInfoRow> SQLiteDatabase::ListClientSysInfo(const SysInfoQuery& q) {
	const auto pattern = GeneralUtils::LikeEscape(q.search, '!');
	auto [_, result] = ExecuteSelect("SELECT c.*, COALESCE(a.name, '') AS account_name" + ClientSysInfoSql::From(q, "'%' || ? || '%'") +
		ClientSysInfoSql::Order(q) + " LIMIT ? OFFSET ?;", q.accountId, q.accountId, q.search, pattern, pattern, q.limit, q.offset);
	std::vector<SysInfoRow> rows;
	for (; !result.eof(); result.nextRow()) rows.push_back(Row(result, true));
	return rows;
}

uint64_t SQLiteDatabase::CountClientSysInfo(const SysInfoQuery& q) {
	const auto pattern = GeneralUtils::LikeEscape(q.search, '!');
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count" + ClientSysInfoSql::From(q, "'%' || ? || '%'") + ";",
		q.accountId, q.accountId, q.search, pattern, pattern);
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("count"));
}
