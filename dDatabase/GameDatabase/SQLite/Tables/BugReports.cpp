#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertNewBugReport(const IBugReports::Info& info) {
	ExecuteInsert("INSERT INTO `bug_reports`(body, client_version, other_player_id, selection, reporter_id) VALUES (?, ?, ?, ?, ?)",
		info.body, info.clientVersion, info.otherPlayer, info.selection, info.characterId);
}
