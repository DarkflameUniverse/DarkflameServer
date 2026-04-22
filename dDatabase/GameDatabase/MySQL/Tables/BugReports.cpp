#include "MySQLDatabase.h"

void MySQLDatabase::InsertNewBugReport(const IBugReports::Info& info) {
	ExecuteInsert("INSERT INTO `bug_reports`(body, client_version, other_player_id, selection, reporter_id) VALUES (?, ?, ?, ?, ?)",
		info.body, info.clientVersion, info.otherPlayer, info.selection, info.characterId);
}

std::vector<IBugReports::DetailedInfo> MySQLDatabase::GetAllBugReports() {
	std::vector<IBugReports::DetailedInfo> out;
	auto res = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"UNIX_TIMESTAMP(submitted) as submitted, UNIX_TIMESTAMP(resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports ORDER BY id DESC;"
	);

	while (res->next()) {
		IBugReports::DetailedInfo info;
		info.id = res->getUInt64("id");
		info.body = res->getString("body").c_str();
		info.clientVersion = res->getString("client_version").c_str();
		info.otherPlayer = res->getString("other_player_id").c_str();
		info.selection = res->getString("selection").c_str();
		info.characterId = res->getUInt64("reporter_id");
		info.submitted = res->getUInt64("submitted");
		info.resolved_time = res->getUInt64("resolved_time");
		info.resolved_by_id = res->getUInt("resolved_by_id");
		info.resolution = res->getString("resolution").c_str();
		out.push_back(info);
	}

	return out;
}

std::vector<IBugReports::DetailedInfo> MySQLDatabase::GetUnresolvedBugReports() {
	std::vector<IBugReports::DetailedInfo> out;
	auto res = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"UNIX_TIMESTAMP(submitted) as submitted, UNIX_TIMESTAMP(resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports WHERE resolved_time IS NULL ORDER BY id DESC;"
	);

	while (res->next()) {
		IBugReports::DetailedInfo info;
		info.id = res->getUInt64("id");
		info.body = res->getString("body").c_str();
		info.clientVersion = res->getString("client_version").c_str();
		info.otherPlayer = res->getString("other_player_id").c_str();
		info.selection = res->getString("selection").c_str();
		info.characterId = res->getUInt64("reporter_id");
		info.submitted = res->getUInt64("submitted");
		info.resolved_time = 0;
		info.resolved_by_id = 0;
		info.resolution = "";
		out.push_back(info);
	}

	return out;
}

std::vector<IBugReports::DetailedInfo> MySQLDatabase::GetResolvedBugReports() {
	std::vector<IBugReports::DetailedInfo> out;
	auto res = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"UNIX_TIMESTAMP(submitted) as submitted, UNIX_TIMESTAMP(resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports WHERE resolved_time IS NOT NULL ORDER BY id DESC;"
	);

	while (res->next()) {
		IBugReports::DetailedInfo info;
		info.id = res->getUInt64("id");
		info.body = res->getString("body").c_str();
		info.clientVersion = res->getString("client_version").c_str();
		info.otherPlayer = res->getString("other_player_id").c_str();
		info.selection = res->getString("selection").c_str();
		info.characterId = res->getUInt64("reporter_id");
		info.submitted = res->getUInt64("submitted");
		info.resolved_time = res->getUInt64("resolved_time");
		info.resolved_by_id = res->getUInt("resolved_by_id");
		info.resolution = res->getString("resolution").c_str();
		out.push_back(info);
	}

	return out;
}

std::optional<IBugReports::DetailedInfo> MySQLDatabase::GetBugReportById(const uint64_t reportId) {
	auto result = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"UNIX_TIMESTAMP(submitted) as submitted, UNIX_TIMESTAMP(resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports WHERE id = ? LIMIT 1;",
		reportId
	);

	if (!result->next()) {
		return std::nullopt;
	}

	IBugReports::DetailedInfo info;
	info.id = result->getUInt64("id");
	info.body = result->getString("body").c_str();
	info.clientVersion = result->getString("client_version").c_str();
	info.otherPlayer = result->getString("other_player_id").c_str();
	info.selection = result->getString("selection").c_str();
	info.characterId = result->getUInt64("reporter_id");
	info.submitted = result->getUInt64("submitted");
	info.resolved_time = result->isNull("resolved_time") ? 0 : result->getUInt64("resolved_time");
	info.resolved_by_id = result->isNull("resolved_by_id") ? 0 : result->getUInt("resolved_by_id");
	info.resolution = result->getString("resolution").c_str();

	return info;
}

void MySQLDatabase::ResolveBugReport(const uint64_t reportId, const uint32_t resolvedById, const std::string_view resolution) {
	ExecuteUpdate(
		"UPDATE bug_reports SET resolved_time = NOW(), resolved_by_id = ?, resolution = ? WHERE id = ?;",
		resolvedById, resolution, reportId
	);
}

uint32_t MySQLDatabase::GetBugReportCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM bug_reports;");
	return res->next() ? res->getUInt("count") : 0;
}

uint32_t MySQLDatabase::GetUnresolvedBugReportCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM bug_reports WHERE resolved_time IS NULL;");
	return res->next() ? res->getUInt("count") : 0;
}
