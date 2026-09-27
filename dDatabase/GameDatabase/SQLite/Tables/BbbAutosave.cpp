#include "SQLiteDatabase.h"

#include "BbbAutosaveItems.h"

std::optional<IBbbAutosave::Info> SQLiteDatabase::GetBbbAutosave(const LWOOBJID characterId) {
	auto [_, result] = ExecuteSelect("SELECT lxfml, source_items, updated_at FROM bbb_autosave WHERE character_id = ?;", characterId);
	if (result.eof()) return std::nullopt;

	IBbbAutosave::Info info;
	int length{};
	const auto* blob = result.getBlobField("lxfml", length);
	if (blob && length > 0) info.lxfml.assign(reinterpret_cast<const char*>(blob), length);
	info.sourceItems = BbbAutosaveItems::Parse(result.getStringField("source_items", ""));
	info.updatedAt = result.getInt64Field("updated_at");
	return info;
}

void SQLiteDatabase::SetBbbAutosave(const LWOOBJID characterId, const IBbbAutosave::Info& info) {
	std::istringstream lxfml(info.lxfml);
	const auto items = BbbAutosaveItems::Join(info.sourceItems);
	ExecuteInsert(
		"INSERT INTO bbb_autosave (character_id, lxfml, source_items, updated_at) VALUES (?, ?, ?, ?) "
		"ON CONFLICT(character_id) DO UPDATE SET lxfml = excluded.lxfml, source_items = excluded.source_items, updated_at = excluded.updated_at;",
		characterId, static_cast<const std::istream*>(&lxfml), items, info.updatedAt);
}

void SQLiteDatabase::DeleteBbbAutosave(const LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM bbb_autosave WHERE character_id = ?;", characterId);
}
