#include "MySQLDatabase.h"

#include "eAccountLink.h"

namespace {
	// The filters are optional; each is written so an empty value turns it off, which keeps one prepared statement
	const std::string REPORTS_WHERE = " WHERE (? < 0 OR status = ?) AND (? = 0 OR target_account_id = ?)";

	template<typename Result> IModeration::PlayerReport ReadReport(Result& r) {
		return { r->getUInt64("id"), r->getInt64("created_at"), std::string(r->getString("kind").c_str()), r->getInt64("reporter_id"),
			r->getUInt("reporter_account_id"), r->getInt64("object_id"), r->getInt("object_lot"), r->getInt64("target_character_id"),
			r->getUInt("target_account_id"), r->getInt64("property_id"), r->getUInt("zone_id"), r->getUInt("instance_id"), r->getUInt("clone_id"),
			std::string(r->getString("body").c_str()), static_cast<uint8_t>(r->getInt("status")), std::string(r->getString("handled_by").c_str()),
			r->getInt64("handled_at"), std::string(r->getString("resolution").c_str()) };
	}

	template<typename Result> IModeration::LinkedAccount ReadLinked(Result& r, eAccountLink link) {
		return { r->getUInt("id"), std::string(r->getString("name").c_str()), static_cast<uint8_t>(r->getInt("gm_level")), r->getBoolean("banned"),
			static_cast<uint8_t>(link), r->getUInt("shared"), r->getInt64("last_seen") };
	}
}

void MySQLDatabase::SetStrikeStep(uint64_t strikeId, const std::string& step, uint32_t count) {
	ExecuteUpdate("UPDATE account_strikes SET step = ?, step_count = ? WHERE id = ?;", step, count, strikeId);
}

std::vector<IModeration::AppliedStrikeStep> MySQLDatabase::GetAppliedStrikeSteps(uint32_t accountId, int64_t since) {
	std::vector<AppliedStrikeStep> steps;
	auto result = ExecuteSelect("SELECT step, step_count, created_at FROM account_strikes WHERE account_id = ? AND step != '' AND created_at >= ? ORDER BY id;", accountId, since);
	while (result->next()) steps.push_back({ std::string(result->getString("step").c_str()), result->getUInt("step_count"), result->getInt64("created_at") });
	return steps;
}

uint64_t MySQLDatabase::InsertPlayerReport(const PlayerReport& r) {
	ExecuteInsert("INSERT INTO player_reports (created_at, kind, reporter_id, reporter_account_id, object_id, object_lot, target_character_id, target_account_id, "
		"property_id, zone_id, instance_id, clone_id, body, resolution) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, '');",
		r.createdAt, r.kind, r.reporterId, r.reporterAccountId, r.objectId, r.objectLot, r.targetCharacterId, r.targetAccountId, r.propertyId,
		r.zoneId, r.instanceId, r.cloneId, r.body);
	auto last = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return last->next() ? last->getUInt64("id") : 0;
}

std::vector<IModeration::PlayerReport> MySQLDatabase::GetPlayerReports(const PlayerReportQuery& q) {
	std::vector<PlayerReport> reports;
	auto result = ExecuteSelect("SELECT * FROM player_reports" + REPORTS_WHERE + " ORDER BY id DESC LIMIT ? OFFSET ?;", q.status, q.status, q.accountId, q.accountId, q.limit, q.offset);
	while (result->next()) reports.push_back(ReadReport(result));
	return reports;
}

uint32_t MySQLDatabase::CountPlayerReports(const PlayerReportQuery& q) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM player_reports" + REPORTS_WHERE + ";", q.status, q.status, q.accountId, q.accountId);
	return result->next() ? result->getUInt("count") : 0;
}

std::optional<IModeration::PlayerReport> MySQLDatabase::GetPlayerReport(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM player_reports WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadReport(result);
}

void MySQLDatabase::SetPlayerReportStatus(uint64_t id, uint8_t status, const std::string& handledBy, const std::string& resolution, int64_t time) {
	ExecuteUpdate("UPDATE player_reports SET status = ?, handled_by = ?, resolution = ?, handled_at = ? WHERE id = ?;", status, handledBy, resolution, time, id);
}

std::vector<IModeration::ChatFilterWord> MySQLDatabase::GetChatFilterWords() {
	std::vector<ChatFilterWord> words;
	auto result = ExecuteSelect("SELECT * FROM chat_filter_words ORDER BY word;");
	while (result->next()) {
		words.push_back({ std::string(result->getString("word").c_str()), result->getInt("allowed") != 0, std::string(result->getString("added_by").c_str()),
			result->getInt64("added_at") });
	}
	return words;
}

void MySQLDatabase::SetChatFilterWord(const ChatFilterWord& w) {
	ExecuteInsert("INSERT INTO chat_filter_words (word, allowed, added_by, added_at) VALUES (?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE allowed = VALUES(allowed), added_by = VALUES(added_by), added_at = VALUES(added_at);", w.word, w.allowed, w.addedBy, w.addedAt);
}

bool MySQLDatabase::DeleteChatFilterWord(const std::string& word) {
	return ExecuteUpdate("DELETE FROM chat_filter_words WHERE word = ?;", word) > 0;
}

void MySQLDatabase::RecordLoginAddress(uint32_t accountId, const std::string& address, int64_t time) {
	ExecuteInsert("INSERT INTO account_login_addresses (account_id, address, first_seen, last_seen) VALUES (?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE last_seen = VALUES(last_seen), logins = logins + 1;", accountId, address, time, time);
}

std::vector<IModeration::LinkedAccount> MySQLDatabase::GetLinkedAccounts(uint32_t accountId) {
	std::vector<LinkedAccount> links;
	auto keys = ExecuteSelect("SELECT a.id, a.name, a.gm_level, a.banned, 0 AS last_seen, "
		"(SELECT COUNT(*) FROM accounts k WHERE k.play_key_id = me.play_key_id) AS shared "
		"FROM accounts me JOIN accounts a ON a.play_key_id = me.play_key_id AND a.id != me.id "
		"WHERE me.id = ? AND me.play_key_id IS NOT NULL AND me.play_key_id != 0 ORDER BY a.id LIMIT 200;", accountId);
	while (keys->next()) links.push_back(ReadLinked(keys, eAccountLink::PLAY_KEY));

	auto emails = ExecuteSelect("SELECT a.id, a.name, a.gm_level, a.banned, 0 AS last_seen, 1 AS shared "
		"FROM accounts me JOIN accounts a ON LOWER(a.email) = LOWER(me.email) AND a.id != me.id "
		"WHERE me.id = ? AND me.email IS NOT NULL AND me.email != '' ORDER BY a.id LIMIT 200;", accountId);
	while (emails->next()) links.push_back(ReadLinked(emails, eAccountLink::EMAIL));

	auto addresses = ExecuteSelect("SELECT a.id, a.name, a.gm_level, a.banned, COUNT(DISTINCT o.address) AS shared, MAX(o.last_seen) AS last_seen "
		"FROM account_login_addresses me JOIN account_login_addresses o ON o.address = me.address AND o.account_id != me.account_id "
		"JOIN accounts a ON a.id = o.account_id WHERE me.account_id = ? GROUP BY a.id, a.name, a.gm_level, a.banned ORDER BY last_seen DESC LIMIT 200;", accountId);
	while (addresses->next()) links.push_back(ReadLinked(addresses, eAccountLink::LOGIN_ADDRESS));
	return links;
}

uint32_t MySQLDatabase::CountLoginAddresses(uint32_t accountId) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM account_login_addresses WHERE account_id = ?;", accountId);
	return result->next() ? result->getUInt("count") : 0;
}
