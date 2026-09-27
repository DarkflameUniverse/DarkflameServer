#include "MySQLDatabase.h"

#include "BbbAutosaveItems.h"

std::optional<IBbbAutosave::Info> MySQLDatabase::GetBbbAutosave(const LWOOBJID characterId) {
	auto result = ExecuteSelect("SELECT lxfml, source_items, updated_at FROM bbb_autosave WHERE character_id = ?;", characterId);
	if (!result->next()) return std::nullopt;

	IBbbAutosave::Info info;
	std::unique_ptr<std::istream> blob(result->getBlob("lxfml"));
	std::stringstream data;
	if (blob) data << blob->rdbuf();
	info.lxfml = data.str();
	info.sourceItems = BbbAutosaveItems::Parse(result->getString("source_items").c_str());
	info.updatedAt = result->getInt64("updated_at");
	return info;
}

void MySQLDatabase::SetBbbAutosave(const LWOOBJID characterId, const IBbbAutosave::Info& info) {
	std::istringstream lxfml(info.lxfml);
	const auto items = BbbAutosaveItems::Join(info.sourceItems);
	ExecuteInsert(
		"INSERT INTO bbb_autosave (character_id, lxfml, source_items, updated_at) VALUES (?, ?, ?, ?) "
		"ON DUPLICATE KEY UPDATE lxfml = VALUES(lxfml), source_items = VALUES(source_items), updated_at = VALUES(updated_at);",
		characterId, static_cast<const std::istream*>(&lxfml), items, info.updatedAt);
}

void MySQLDatabase::DeleteBbbAutosave(const LWOOBJID characterId) {
	ExecuteDelete("DELETE FROM bbb_autosave WHERE character_id = ?;", characterId);
}
