#include "SQLiteDatabase.h"

namespace {
	IAccountStrikes::Strike ReadStrike(CppSQLite3Query& result) {
		return { static_cast<uint64_t>(result.getInt64Field("id")), static_cast<uint32_t>(result.getIntField("account_id")),
			result.getInt64Field("character_id"), result.getStringField("source"), result.getStringField("subject"), result.getStringField("reason"),
			static_cast<uint32_t>(result.getIntField("given_by_id")), result.getStringField("given_by"), result.getInt64Field("created_at"),
			result.getInt64Field("revoked_at"), result.getStringField("revoked_by"), result.getStringField("revoke_reason") };
	}
}

uint64_t SQLiteDatabase::InsertStrike(const Strike& strike) {
	ExecuteInsert("INSERT INTO account_strikes (account_id, character_id, source, subject, reason, given_by_id, given_by, created_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
		strike.accountId, strike.characterId, strike.source, strike.subject, strike.reason, strike.givenById, strike.givenBy, strike.createdAt);
	auto [_, result] = ExecuteSelect("SELECT last_insert_rowid() AS id;");
	return result.eof() ? 0 : static_cast<uint64_t>(result.getInt64Field("id"));
}

std::vector<IAccountStrikes::Strike> SQLiteDatabase::GetStrikes(uint32_t accountId) {
	std::vector<Strike> strikes;
	auto [_, result] = ExecuteSelect("SELECT * FROM account_strikes WHERE account_id = ? ORDER BY id DESC;", accountId);
	for (; !result.eof(); result.nextRow()) strikes.push_back(ReadStrike(result));
	return strikes;
}

std::optional<IAccountStrikes::Strike> SQLiteDatabase::GetStrike(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM account_strikes WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return ReadStrike(result);
}

void SQLiteDatabase::RevokeStrike(uint64_t id, const std::string& revokedBy, const std::string& reason, int64_t time) {
	ExecuteUpdate("UPDATE account_strikes SET revoked_at = ?, revoked_by = ?, revoke_reason = ? WHERE id = ? AND revoked_at = 0;", time, revokedBy, reason, static_cast<int64_t>(id));
}

uint32_t SQLiteDatabase::CountActiveStrikes(uint32_t accountId, int64_t since) {
	auto [_, result] = accountId == 0
		? ExecuteSelect("SELECT COUNT(*) AS count FROM account_strikes WHERE revoked_at = 0 AND created_at >= ?;", since)
		: ExecuteSelect("SELECT COUNT(*) AS count FROM account_strikes WHERE account_id = ? AND revoked_at = 0 AND created_at >= ?;", accountId, since);
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
}
