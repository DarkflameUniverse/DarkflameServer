#include "MySQLDatabase.h"

namespace {
	template<typename Result> IAccountStrikes::Strike ReadStrike(Result& result) {
		return { result->getUInt64("id"), result->getUInt("account_id"), result->getInt64("character_id"), result->getString("source").c_str(),
			result->getString("subject").c_str(), result->getString("reason").c_str(), result->getUInt("given_by_id"), result->getString("given_by").c_str(),
			result->getInt64("created_at"), result->getInt64("revoked_at"), result->getString("revoked_by").c_str(), result->getString("revoke_reason").c_str() };
	}
}

uint64_t MySQLDatabase::InsertStrike(const Strike& strike) {
	ExecuteInsert("INSERT INTO account_strikes (account_id, character_id, source, subject, reason, given_by_id, given_by, created_at, revoke_reason) VALUES (?, ?, ?, ?, ?, ?, ?, ?, '');",
		strike.accountId, strike.characterId, strike.source, strike.subject, strike.reason, strike.givenById, strike.givenBy, strike.createdAt);
	auto result = ExecuteSelect("SELECT LAST_INSERT_ID() AS id;");
	return result->next() ? result->getUInt64("id") : 0;
}

std::vector<IAccountStrikes::Strike> MySQLDatabase::GetStrikes(uint32_t accountId) {
	std::vector<Strike> strikes;
	auto result = ExecuteSelect("SELECT * FROM account_strikes WHERE account_id = ? ORDER BY id DESC;", accountId);
	while (result->next()) strikes.push_back(ReadStrike(result));
	return strikes;
}

std::optional<IAccountStrikes::Strike> MySQLDatabase::GetStrike(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM account_strikes WHERE id = ?;", id);
	if (!result->next()) return std::nullopt;
	return ReadStrike(result);
}

void MySQLDatabase::RevokeStrike(uint64_t id, const std::string& revokedBy, const std::string& reason, int64_t time) {
	ExecuteUpdate("UPDATE account_strikes SET revoked_at = ?, revoked_by = ?, revoke_reason = ? WHERE id = ? AND revoked_at = 0;", time, revokedBy, reason, id);
}

uint32_t MySQLDatabase::CountActiveStrikes(uint32_t accountId, int64_t since) {
	auto result = accountId == 0
		? ExecuteSelect("SELECT COUNT(*) AS count FROM account_strikes WHERE revoked_at = 0 AND created_at >= ?;", since)
		: ExecuteSelect("SELECT COUNT(*) AS count FROM account_strikes WHERE account_id = ? AND revoked_at = 0 AND created_at >= ?;", accountId, since);
	return result->next() ? result->getUInt("count") : 0;
}
