#include "SQLiteDatabase.h"

std::optional<bool> SQLiteDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto [_, keyCheckRes] = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (keyCheckRes.eof()) {
		return std::nullopt;
	}

	return keyCheckRes.getIntField("active");
}

std::vector<IPlayKeys::Info> SQLiteDatabase::GetAllPlayKeys() {
	std::vector<IPlayKeys::Info> out;
	auto [stmt, res] = ExecuteSelect(
		"SELECT id, key_string, key_uses, times_used, active, notes, strftime('%s', created_at) as created_at "
		"FROM play_keys ORDER BY id DESC;"
	);

	while (!res.eof()) {
		IPlayKeys::Info info;
		info.id = res.getIntField("id");
		info.key_string = res.getStringField("key_string");
		info.key_uses = res.getIntField("key_uses");
		info.times_used = res.getIntField("times_used");
		info.active = res.getIntField("active") != 0;
		info.notes = res.getStringField("notes");
		info.created_at = static_cast<uint64_t>(res.getInt64Field("created_at"));
		out.push_back(info);
		res.nextRow();
	}

	return out;
}

std::optional<IPlayKeys::Info> SQLiteDatabase::GetPlayKeyById(const uint32_t playkeyId) {
	auto [_, result] = ExecuteSelect(
		"SELECT id, key_string, key_uses, times_used, active, notes, strftime('%s', created_at) as created_at "
		"FROM play_keys WHERE id = ? LIMIT 1;",
		playkeyId
	);

	if (result.eof()) {
		return std::nullopt;
	}

	IPlayKeys::Info info;
	info.id = result.getIntField("id");
	info.key_string = result.getStringField("key_string");
	info.key_uses = result.getIntField("key_uses");
	info.times_used = result.getIntField("times_used");
	info.active = result.getIntField("active") != 0;
	info.notes = result.getStringField("notes");
	info.created_at = static_cast<uint64_t>(result.getInt64Field("created_at"));

	return info;
}

std::optional<IPlayKeys::Info> SQLiteDatabase::GetPlayKeyByString(const std::string_view key_string) {
	auto [_, result] = ExecuteSelect(
		"SELECT id, key_string, key_uses, times_used, active, notes, strftime('%s', created_at) as created_at "
		"FROM play_keys WHERE key_string = ? LIMIT 1;",
		key_string
	);

	if (result.eof()) {
		return std::nullopt;
	}

	IPlayKeys::Info info;
	info.id = result.getIntField("id");
	info.key_string = result.getStringField("key_string");
	info.key_uses = result.getIntField("key_uses");
	info.times_used = result.getIntField("times_used");
	info.active = result.getIntField("active") != 0;
	info.notes = result.getStringField("notes");
	info.created_at = static_cast<uint64_t>(result.getInt64Field("created_at"));

	return info;
}

bool SQLiteDatabase::ConsumePlayKeyUsage(const uint32_t playkeyId) {
	// Atomically check times_used < key_uses and increment times_used. If uses exhausted, deactivate.
	auto [stmt, res] = ExecuteSelect("SELECT key_uses, times_used, active FROM play_keys WHERE id = ? LIMIT 1;", playkeyId);
	if (res.eof()) return false;

	int key_uses = res.getIntField("key_uses");
	int times_used = res.getIntField("times_used");
	int active = res.getIntField("active");

	if (!active) return false;
	if (times_used >= key_uses) return false;

	// Increment times_used
	ExecuteUpdate("UPDATE play_keys SET times_used = times_used + 1 WHERE id = ?;", playkeyId);

	// If we've reached the limit, deactivate
	if (times_used + 1 >= key_uses) {
		ExecuteUpdate("UPDATE play_keys SET active = 0 WHERE id = ?;", playkeyId);
	}

	return true;
}

void SQLiteDatabase::CreatePlayKey(const std::string_view key_string, uint32_t uses, const std::string_view notes) {
	ExecuteInsert(
		"INSERT INTO play_keys (key_string, key_uses, times_used, active, notes) VALUES (?, ?, 0, 1, ?);",
		key_string, uses, notes
	);
}

void SQLiteDatabase::UpdatePlayKey(const uint32_t playkeyId, uint32_t uses, bool active, const std::string_view notes) {
	ExecuteUpdate(
		"UPDATE play_keys SET key_uses = ?, active = ?, notes = ? WHERE id = ?;",
		uses, active ? 1 : 0, notes, playkeyId
	);
}

void SQLiteDatabase::DeletePlayKey(const uint32_t playkeyId) {
	ExecuteDelete("DELETE FROM play_keys WHERE id = ?;", playkeyId);
}

uint32_t SQLiteDatabase::GetPlayKeyCount() {
	auto [_, res] = ExecuteSelect("SELECT COUNT(*) as count FROM play_keys;");
	return res.eof() ? 0 : res.getIntField("count");
}
