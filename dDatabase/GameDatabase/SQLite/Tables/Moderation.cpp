#include "SQLiteDatabase.h"

#include "eAccountLink.h"

namespace {
	// The filters are optional; each is written so an empty value turns it off, which keeps one prepared statement
	const std::string REPORTS_WHERE = " WHERE (? < 0 OR status = ?) AND (? = 0 OR target_account_id = ?)";

	IModeration::PlayerReport ReadReport(CppSQLite3Query& r) {
		return { static_cast<uint64_t>(r.getInt64Field("id")), r.getInt64Field("created_at"), r.getStringField("kind"), r.getInt64Field("reporter_id"),
			static_cast<uint32_t>(r.getIntField("reporter_account_id")), r.getInt64Field("object_id"), r.getIntField("object_lot"),
			r.getInt64Field("target_character_id"), static_cast<uint32_t>(r.getIntField("target_account_id")), r.getInt64Field("property_id"),
			static_cast<uint32_t>(r.getIntField("zone_id")), static_cast<uint32_t>(r.getIntField("instance_id")), static_cast<uint32_t>(r.getIntField("clone_id")),
			r.getStringField("body"), static_cast<uint8_t>(r.getIntField("status")), r.getStringField("handled_by"), r.getInt64Field("handled_at"),
			r.getStringField("resolution") };
	}

	IModeration::LinkedAccount ReadLinked(CppSQLite3Query& r, eAccountLink link) {
		return { static_cast<uint32_t>(r.getIntField("id")), r.getStringField("name"), static_cast<uint8_t>(r.getIntField("gm_level")),
			r.getIntField("banned") != 0, static_cast<uint8_t>(link), static_cast<uint32_t>(r.getIntField("shared")), r.getInt64Field("last_seen") };
	}
}

void SQLiteDatabase::SetStrikeStep(uint64_t strikeId, const std::string& step, uint32_t count) {
	ExecuteUpdate("UPDATE account_strikes SET step = ?, step_count = ? WHERE id = ?;", step, count, static_cast<int64_t>(strikeId));
}

std::vector<IModeration::AppliedStrikeStep> SQLiteDatabase::GetAppliedStrikeSteps(uint32_t accountId, int64_t since) {
	std::vector<AppliedStrikeStep> steps;
	auto [_, result] = ExecuteSelect("SELECT step, step_count, created_at FROM account_strikes WHERE account_id = ? AND step != '' AND created_at >= ? ORDER BY id;", accountId, since);
	for (; !result.eof(); result.nextRow()) steps.push_back({ result.getStringField("step"), static_cast<uint32_t>(result.getIntField("step_count")), result.getInt64Field("created_at") });
	return steps;
}

uint64_t SQLiteDatabase::InsertPlayerReport(const PlayerReport& r) {
	ExecuteInsert("INSERT INTO player_reports (created_at, kind, reporter_id, reporter_account_id, object_id, object_lot, target_character_id, target_account_id, "
		"property_id, zone_id, instance_id, clone_id, body) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
		r.createdAt, r.kind, r.reporterId, r.reporterAccountId, r.objectId, r.objectLot, r.targetCharacterId, r.targetAccountId, r.propertyId,
		r.zoneId, r.instanceId, r.cloneId, r.body);
	auto [_, last] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return last.eof() ? 0 : static_cast<uint64_t>(last.getInt64Field("id"));
}

std::vector<IModeration::PlayerReport> SQLiteDatabase::GetPlayerReports(const PlayerReportQuery& q) {
	std::vector<PlayerReport> reports;
	auto [_, result] = ExecuteSelect("SELECT * FROM player_reports" + REPORTS_WHERE + " ORDER BY id DESC LIMIT ? OFFSET ?;", q.status, q.status, q.accountId, q.accountId, q.limit, q.offset);
	for (; !result.eof(); result.nextRow()) reports.push_back(ReadReport(result));
	return reports;
}

uint32_t SQLiteDatabase::CountPlayerReports(const PlayerReportQuery& q) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM player_reports" + REPORTS_WHERE + ";", q.status, q.status, q.accountId, q.accountId);
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
}

std::optional<IModeration::PlayerReport> SQLiteDatabase::GetPlayerReport(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM player_reports WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return ReadReport(result);
}

void SQLiteDatabase::SetPlayerReportStatus(uint64_t id, uint8_t status, const std::string& handledBy, const std::string& resolution, int64_t time) {
	ExecuteUpdate("UPDATE player_reports SET status = ?, handled_by = ?, resolution = ?, handled_at = ? WHERE id = ?;", status, handledBy, resolution, time, static_cast<int64_t>(id));
}

std::vector<IModeration::ChatFilterWord> SQLiteDatabase::GetChatFilterWords() {
	std::vector<ChatFilterWord> words;
	auto [_, result] = ExecuteSelect("SELECT * FROM chat_filter_words ORDER BY word;");
	for (; !result.eof(); result.nextRow()) {
		words.push_back({ result.getStringField("word"), result.getIntField("allowed") != 0, result.getStringField("added_by"), result.getInt64Field("added_at") });
	}
	return words;
}

void SQLiteDatabase::SetChatFilterWord(const ChatFilterWord& w) {
	ExecuteInsert("INSERT INTO chat_filter_words (word, allowed, added_by, added_at) VALUES (?, ?, ?, ?) "
		"ON CONFLICT(word) DO UPDATE SET allowed = excluded.allowed, added_by = excluded.added_by, added_at = excluded.added_at;", w.word, w.allowed, w.addedBy, w.addedAt);
}

bool SQLiteDatabase::DeleteChatFilterWord(const std::string& word) {
	return ExecuteUpdate("DELETE FROM chat_filter_words WHERE word = ?;", word) > 0;
}

void SQLiteDatabase::RecordLoginAddress(uint32_t accountId, const std::string& address, int64_t time) {
	ExecuteInsert("INSERT INTO account_login_addresses (account_id, address, first_seen, last_seen) VALUES (?, ?, ?, ?) "
		"ON CONFLICT(account_id, address) DO UPDATE SET last_seen = excluded.last_seen, logins = logins + 1;", accountId, address, time, time);
}

std::vector<IModeration::LinkedAccount> SQLiteDatabase::GetLinkedAccounts(uint32_t accountId) {
	std::vector<LinkedAccount> links;
	{
		auto [_, result] = ExecuteSelect("SELECT a.id, a.name, a.gm_level, a.banned, 0 AS last_seen, "
			"(SELECT COUNT(*) FROM accounts k WHERE k.play_key_id = me.play_key_id) AS shared "
			"FROM accounts me JOIN accounts a ON a.play_key_id = me.play_key_id AND a.id != me.id "
			"WHERE me.id = ? AND me.play_key_id IS NOT NULL AND me.play_key_id != 0 ORDER BY a.id LIMIT 200;", accountId);
		for (; !result.eof(); result.nextRow()) links.push_back(ReadLinked(result, eAccountLink::PLAY_KEY));
	}
	{
		auto [_, result] = ExecuteSelect("SELECT a.id, a.name, a.gm_level, a.banned, 0 AS last_seen, 1 AS shared "
			"FROM accounts me JOIN accounts a ON LOWER(a.email) = LOWER(me.email) AND a.id != me.id "
			"WHERE me.id = ? AND me.email IS NOT NULL AND me.email != '' ORDER BY a.id LIMIT 200;", accountId);
		for (; !result.eof(); result.nextRow()) links.push_back(ReadLinked(result, eAccountLink::EMAIL));
	}
	{
		auto [_, result] = ExecuteSelect("SELECT a.id, a.name, a.gm_level, a.banned, COUNT(DISTINCT o.address) AS shared, MAX(o.last_seen) AS last_seen "
			"FROM account_login_addresses me JOIN account_login_addresses o ON o.address = me.address AND o.account_id != me.account_id "
			"JOIN accounts a ON a.id = o.account_id WHERE me.account_id = ? GROUP BY a.id, a.name, a.gm_level, a.banned ORDER BY last_seen DESC LIMIT 200;", accountId);
		for (; !result.eof(); result.nextRow()) links.push_back(ReadLinked(result, eAccountLink::LOGIN_ADDRESS));
	}
	return links;
}

uint32_t SQLiteDatabase::CountLoginAddresses(uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM account_login_addresses WHERE account_id = ?;", accountId);
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
}
