#include "MySQLDatabase.h"

void MySQLDatabase::InsertNewBugReport(const IBugReports::Info& info) {
	ExecuteInsert("INSERT INTO `bug_reports`(body, client_version, other_player_id, selection, reporter_id) VALUES (?, ?, ?, ?, ?)",
		info.body, info.clientVersion, info.otherPlayer, info.selection, info.characterId);
}

#include "json.hpp"

void MySQLDatabase::DeleteBugReport(const uint32_t id) {
	ExecuteUpdate("DELETE FROM bug_reports WHERE id = ?;", id);
}

uint32_t MySQLDatabase::GetBugReportCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM bug_reports;");
	return res->next() ? res->getUInt("count") : 0;
}

