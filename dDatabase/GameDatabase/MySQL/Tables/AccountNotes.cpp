#include "MySQLDatabase.h"

void MySQLDatabase::InsertAccountNote(const AccountNote& note) {
	ExecuteInsert("INSERT INTO account_notes (account_id, kind, text, actor, created_at) VALUES (?, ?, ?, ?, ?);", note.accountId, note.kind, note.text, note.actor, note.createdAt);
}

void MySQLDatabase::DeleteAccountNote(uint64_t id) {
	ExecuteDelete("DELETE FROM account_notes WHERE id = ?;", static_cast<int64_t>(id));
}

void MySQLDatabase::SetAccountBan(uint32_t accountId, bool banned, int64_t expires, const std::string& reason) {
	ExecuteUpdate("UPDATE accounts SET banned = ?, ban_expires = ?, ban_reason = ? WHERE id = ?;", banned, banned ? expires : int64_t{ 0 }, banned ? reason : std::string(), accountId);
}

std::vector<IAccountNotes::AccountNote> MySQLDatabase::GetAccountNotes(uint32_t accountId) {
	std::vector<AccountNote> notes;
	auto result = ExecuteSelect("SELECT * FROM account_notes WHERE account_id = ? ORDER BY id DESC;", accountId);
	while (result->next()) {
		notes.push_back({ result->getUInt64("id"), result->getUInt("account_id"), result->getString("kind").c_str(),
			result->getString("text").c_str(), result->getString("actor").c_str(), result->getInt64("created_at") });
	}
	return notes;
}

std::optional<IAccountNotes::AccountNote> MySQLDatabase::GetAccountNote(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM account_notes WHERE id = ?;", static_cast<int64_t>(id));
	if (!result->next()) return std::nullopt;
	return AccountNote{ result->getUInt64("id"), result->getUInt("account_id"), result->getString("kind").c_str(),
		result->getString("text").c_str(), result->getString("actor").c_str(), result->getInt64("created_at") };
}

uint32_t MySQLDatabase::GetAccountWarningCount(uint32_t accountId) {
	auto result = ExecuteSelect("SELECT COUNT(*) AS count FROM account_notes WHERE account_id = ? AND kind = 'warning';", accountId);
	return result->next() ? result->getUInt("count") : 0;
}

std::vector<uint32_t> MySQLDatabase::LiftExpiredBans(int64_t now) {
	std::vector<uint32_t> ids;
	{
		auto result = ExecuteSelect("SELECT id FROM accounts WHERE banned = 1 AND ban_expires > 0 AND ban_expires <= ?;", now);
		while (result->next()) ids.push_back(result->getUInt("id"));
	}
	if (!ids.empty()) ExecuteUpdate("UPDATE accounts SET banned = 0, ban_expires = 0, ban_reason = NULL WHERE banned = 1 AND ban_expires > 0 AND ban_expires <= ?;", now);
	return ids;
}
