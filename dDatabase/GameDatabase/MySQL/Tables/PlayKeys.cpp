#include "MySQLDatabase.h"

std::optional<bool> MySQLDatabase::IsPlaykeyActive(const int32_t playkeyId) {
	auto keyCheckRes = ExecuteSelect("SELECT active FROM `play_keys` WHERE id=?", playkeyId);

	if (!keyCheckRes->next()) {
		return std::nullopt;
	}

	return keyCheckRes->getBoolean("active");
}

std::vector<IPlayKeys::Info> MySQLDatabase::GetAllPlayKeys() {
	std::vector<IPlayKeys::Info> out;
	auto res = ExecuteSelect(
		"SELECT id, key_string, key_uses, times_used, active, notes, UNIX_TIMESTAMP(created_at) as created_at "
		"FROM play_keys ORDER BY id DESC;"
	);

	while (res->next()) {
		IPlayKeys::Info info;
		info.id = res->getUInt("id");
		info.key_string = res->getString("key_string").c_str();
		info.key_uses = res->getUInt("key_uses");
		info.times_used = res->getUInt("times_used");
		info.active = res->getBoolean("active");
		info.notes = res->getString("notes").c_str();
		info.created_at = res->getUInt64("created_at");
		out.push_back(info);
	}

	return out;
}

std::optional<IPlayKeys::Info> MySQLDatabase::GetPlayKeyById(const uint32_t playkeyId) {
	auto result = ExecuteSelect(
		"SELECT id, key_string, key_uses, times_used, active, notes, UNIX_TIMESTAMP(created_at) as created_at "
		"FROM play_keys WHERE id = ? LIMIT 1;",
		playkeyId
	);

	if (!result->next()) {
		return std::nullopt;
	}

	IPlayKeys::Info info;
	info.id = result->getUInt("id");
	info.key_string = result->getString("key_string").c_str();
	info.key_uses = result->getUInt("key_uses");
	info.times_used = result->getUInt("times_used");
	info.active = result->getBoolean("active");
	info.notes = result->getString("notes").c_str();
	info.created_at = result->getUInt64("created_at");

	return info;
}

std::optional<IPlayKeys::Info> MySQLDatabase::GetPlayKeyByString(const std::string_view key_string) {
	auto result = ExecuteSelect(
		"SELECT id, key_string, key_uses, times_used, active, notes, UNIX_TIMESTAMP(created_at) as created_at "
		"FROM play_keys WHERE key_string = ? LIMIT 1;",
		key_string
	);

	if (!result->next()) {
		return std::nullopt;
	}

	IPlayKeys::Info info;
	info.id = result->getUInt("id");
	info.key_string = result->getString("key_string").c_str();
	info.key_uses = result->getUInt("key_uses");
	info.times_used = result->getUInt("times_used");
	info.active = result->getBoolean("active");
	info.notes = result->getString("notes").c_str();
	info.created_at = result->getUInt64("created_at");

	return info;
}

bool MySQLDatabase::ConsumePlayKeyUsage(const uint32_t playkeyId) {
	// Check current state
	auto res = ExecuteSelect("SELECT key_uses, times_used, active FROM play_keys WHERE id = ? LIMIT 1;", playkeyId);
	if (!res->next()) return false;

	int key_uses = res->getUInt("key_uses");
	int times_used = res->getUInt("times_used");
	bool active = res->getBoolean("active");

	if (!active) return false;
	if (times_used >= key_uses) return false;

	// Increment times_used
	ExecuteUpdate("UPDATE play_keys SET times_used = times_used + 1 WHERE id = ?;", playkeyId);

	// Deactivate if used up
	if (times_used + 1 >= key_uses) {
		ExecuteUpdate("UPDATE play_keys SET active = 0 WHERE id = ?;", playkeyId);
	}

	return true;
}

void MySQLDatabase::CreatePlayKey(const std::string_view key_string, uint32_t uses, const std::string_view notes) {
	ExecuteInsert(
		"INSERT INTO play_keys (key_string, key_uses, times_used, active, notes) VALUES (?, ?, 0, 1, ?);",
		key_string, uses, notes
	);
}

void MySQLDatabase::UpdatePlayKey(const uint32_t playkeyId, uint32_t uses, bool active, const std::string_view notes) {
	ExecuteUpdate(
		"UPDATE play_keys SET key_uses = ?, active = ?, notes = ? WHERE id = ?;",
		uses, active, notes, playkeyId
	);
}

void MySQLDatabase::DeletePlayKey(const uint32_t playkeyId) {
	ExecuteDelete("DELETE FROM play_keys WHERE id = ?;", playkeyId);
}

uint32_t MySQLDatabase::GetPlayKeyCount() {
	auto res = ExecuteSelect("SELECT COUNT(*) as count FROM play_keys;");
	return res->next() ? res->getUInt("count") : 0;
}
