#include "SQLiteDatabase.h"

void SQLiteDatabase::InsertAccountNote(const AccountNote& note) {
	ExecuteInsert("INSERT INTO account_notes (account_id, kind, text, actor, created_at) VALUES (?, ?, ?, ?, ?);", note.accountId, note.kind, note.text, note.actor, note.createdAt);
}

void SQLiteDatabase::DeleteAccountNote(uint64_t id) {
	ExecuteDelete("DELETE FROM account_notes WHERE id = ?;", static_cast<int64_t>(id));
}

void SQLiteDatabase::SetAccountBan(uint32_t accountId, bool banned, int64_t expires, const std::string& reason) {
	ExecuteUpdate("UPDATE accounts SET banned = ?, ban_expires = ?, ban_reason = ? WHERE id = ?;", banned, banned ? expires : int64_t{ 0 }, banned ? reason : std::string(), accountId);
}

std::vector<IAccountNotes::AccountNote> SQLiteDatabase::GetAccountNotes(uint32_t accountId) {
	std::vector<AccountNote> notes;
	auto [_, result] = ExecuteSelect("SELECT * FROM account_notes WHERE account_id = ? ORDER BY id DESC;", accountId);
	while (!result.eof()) {
		notes.push_back({ static_cast<uint64_t>(result.getInt64Field("id")), static_cast<uint32_t>(result.getIntField("account_id")), result.getStringField("kind"),
			result.getStringField("text"), result.getStringField("actor"), result.getInt64Field("created_at") });
		result.nextRow();
	}
	return notes;
}

std::optional<IAccountNotes::AccountNote> SQLiteDatabase::GetAccountNote(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM account_notes WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return AccountNote{ static_cast<uint64_t>(result.getInt64Field("id")), static_cast<uint32_t>(result.getIntField("account_id")), result.getStringField("kind"),
		result.getStringField("text"), result.getStringField("actor"), result.getInt64Field("created_at") };
}

uint32_t SQLiteDatabase::GetAccountWarningCount(uint32_t accountId) {
	auto [_, result] = ExecuteSelect("SELECT COUNT(*) AS count FROM account_notes WHERE account_id = ? AND kind = 'warning';", accountId);
	return result.eof() ? 0 : static_cast<uint32_t>(result.getIntField("count"));
}

std::vector<uint32_t> SQLiteDatabase::LiftExpiredBans(int64_t now) {
	std::vector<uint32_t> ids;
	{
		auto [_, result] = ExecuteSelect("SELECT id FROM accounts WHERE banned = 1 AND ban_expires > 0 AND ban_expires <= ?;", now);
		while (!result.eof()) { ids.push_back(static_cast<uint32_t>(result.getIntField("id"))); result.nextRow(); }
	}
	if (!ids.empty()) ExecuteUpdate("UPDATE accounts SET banned = 0, ban_expires = 0, ban_reason = NULL WHERE banned = 1 AND ban_expires > 0 AND ban_expires <= ?;", now);
	return ids;
}
