#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertNewBugReport(const IBugReports::Info& info) {
	ExecuteInsert("INSERT INTO `bug_reports`(body, client_version, other_player_id, selection, reporter_id) VALUES (?, ?, ?, ?, ?)",
		info.body, info.clientVersion, info.otherPlayer, info.selection, info.characterId);
}

std::vector<IBugReports::DetailedInfo> SQLiteDatabase::GetAllBugReports() {
	std::vector<IBugReports::DetailedInfo> out;
	auto [stmt, res] = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"strftime('%s', submitted) as submitted, strftime('%s', resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports ORDER BY id DESC;"
	);

	while (!res.eof()) {
		IBugReports::DetailedInfo info;
		info.id = static_cast<uint64_t>(res.getInt64Field("id"));
		info.body = res.getStringField("body");
		info.clientVersion = res.getStringField("client_version");
		info.otherPlayer = res.getStringField("other_player_id");
		info.selection = res.getStringField("selection");
		info.characterId = static_cast<LWOOBJID>(res.getInt64Field("reporter_id"));
		info.submitted = static_cast<uint64_t>(res.getInt64Field("submitted"));
		info.resolved_time = res.fieldIsNull("resolved_time") ? 0 : static_cast<uint64_t>(res.getInt64Field("resolved_time"));
		info.resolved_by_id = res.fieldIsNull("resolved_by_id") ? 0 : res.getIntField("resolved_by_id");
		info.resolution = res.getStringField("resolution");
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

std::vector<IBugReports::DetailedInfo> SQLiteDatabase::GetUnresolvedBugReports() {
	std::vector<IBugReports::DetailedInfo> out;
	auto [stmt, res] = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"strftime('%s', submitted) as submitted FROM bug_reports WHERE resolved_time IS NULL ORDER BY id DESC;"
	);

	while (!res.eof()) {
		IBugReports::DetailedInfo info;
		info.id = static_cast<uint64_t>(res.getInt64Field("id"));
		info.body = res.getStringField("body");
		info.clientVersion = res.getStringField("client_version");
		info.otherPlayer = res.getStringField("other_player_id");
		info.selection = res.getStringField("selection");
		info.characterId = static_cast<LWOOBJID>(res.getInt64Field("reporter_id"));
		info.submitted = static_cast<uint64_t>(res.getInt64Field("submitted"));
		info.resolved_time = 0;
		info.resolved_by_id = 0;
		info.resolution = "";
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

std::vector<IBugReports::DetailedInfo> SQLiteDatabase::GetResolvedBugReports() {
	std::vector<IBugReports::DetailedInfo> out;
	auto [stmt, res] = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"strftime('%s', submitted) as submitted, strftime('%s', resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports WHERE resolved_time IS NOT NULL ORDER BY id DESC;"
	);

	while (!res.eof()) {
		IBugReports::DetailedInfo info;
		info.id = static_cast<uint64_t>(res.getInt64Field("id"));
		info.body = res.getStringField("body");
		info.clientVersion = res.getStringField("client_version");
		info.otherPlayer = res.getStringField("other_player_id");
		info.selection = res.getStringField("selection");
		info.characterId = static_cast<LWOOBJID>(res.getInt64Field("reporter_id"));
		info.submitted = static_cast<uint64_t>(res.getInt64Field("submitted"));
		info.resolved_time = static_cast<uint64_t>(res.getInt64Field("resolved_time"));
		info.resolved_by_id = res.getIntField("resolved_by_id");
		info.resolution = res.getStringField("resolution");
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

std::optional<IBugReports::DetailedInfo> SQLiteDatabase::GetBugReportById(const uint64_t reportId) {
	auto [_, result] = ExecuteSelect(
		"SELECT id, body, client_version, other_player_id, selection, reporter_id, "
		"strftime('%s', submitted) as submitted, strftime('%s', resolved_time) as resolved_time, "
		"resolved_by_id, resolution FROM bug_reports WHERE id = ? LIMIT 1;",
		reportId
	);

	if (result.eof()) {
		return std::nullopt;
	}

	IBugReports::DetailedInfo info;
	info.id = static_cast<uint64_t>(result.getInt64Field("id"));
	info.body = result.getStringField("body");
	info.clientVersion = result.getStringField("client_version");
	info.otherPlayer = result.getStringField("other_player_id");
	info.selection = result.getStringField("selection");
	info.characterId = static_cast<LWOOBJID>(result.getInt64Field("reporter_id"));
	info.submitted = static_cast<uint64_t>(result.getInt64Field("submitted"));
	info.resolved_time = result.fieldIsNull("resolved_time") ? 0 : static_cast<uint64_t>(result.getInt64Field("resolved_time"));
	info.resolved_by_id = result.fieldIsNull("resolved_by_id") ? 0 : result.getIntField("resolved_by_id");
	info.resolution = result.getStringField("resolution");

	return info;
}

void SQLiteDatabase::ResolveBugReport(const uint64_t reportId, const uint32_t resolvedById, const std::string_view resolution) {
	ExecuteUpdate(
		"UPDATE bug_reports SET resolved_time = datetime('now'), resolved_by_id = ?, resolution = ? WHERE id = ?;",
		resolvedById, resolution, reportId
	);
}

uint32_t SQLiteDatabase::GetBugReportCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM bug_reports;");
	return res.eof() ? 0 : res.getIntField("count");
}

uint32_t SQLiteDatabase::GetUnresolvedBugReportCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM bug_reports WHERE resolved_time IS NULL;");
	return res.eof() ? 0 : res.getIntField("count");
}
