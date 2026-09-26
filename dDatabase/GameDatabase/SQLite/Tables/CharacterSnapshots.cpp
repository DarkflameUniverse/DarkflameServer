#include "SQLiteDatabase.h"

#include <sstream>

namespace {
	ICharacterSnapshots::CharacterSnapshot Row(CppSQLite3Query& result, bool withXml) {
		ICharacterSnapshots::CharacterSnapshot snapshot;
		snapshot.id = static_cast<uint64_t>(result.getInt64Field("id"));
		snapshot.characterId = result.getInt64Field("character_id");
		snapshot.takenAt = result.getInt64Field("taken_at");
		snapshot.reason = result.getStringField("reason");
		snapshot.actor = result.getStringField("actor");
		snapshot.size = static_cast<uint32_t>(result.getInt64Field("size"));
		snapshot.hash = result.getStringField("hash");
		if (withXml) {
			int length = 0;
			const auto* blob = result.getBlobField("xml", length);
			snapshot.compressed.assign(reinterpret_cast<const char*>(blob), length);
		}
		return snapshot;
	}
}

void SQLiteDatabase::InsertCharacterSnapshot(const CharacterSnapshot& snapshot) {
	std::istringstream xml(snapshot.compressed);
	ExecuteInsert("INSERT INTO character_snapshots (character_id, taken_at, reason, actor, size, hash, xml) VALUES (?, ?, ?, ?, ?, ?, ?);",
		snapshot.characterId, snapshot.takenAt, snapshot.reason, snapshot.actor, snapshot.size, snapshot.hash, static_cast<const std::istream*>(&xml));
}

std::vector<ICharacterSnapshots::CharacterSnapshot> SQLiteDatabase::GetCharacterSnapshots(LWOOBJID characterId) {
	std::vector<CharacterSnapshot> snapshots;
	auto [_, result] = ExecuteSelect("SELECT id, character_id, taken_at, reason, actor, size, hash FROM character_snapshots WHERE character_id = ? ORDER BY id DESC;", characterId);
	while (!result.eof()) {
		snapshots.push_back(Row(result, false));
		result.nextRow();
	}
	return snapshots;
}

std::optional<ICharacterSnapshots::CharacterSnapshot> SQLiteDatabase::GetCharacterSnapshot(uint64_t id) {
	auto [_, result] = ExecuteSelect("SELECT * FROM character_snapshots WHERE id = ?;", static_cast<int64_t>(id));
	if (result.eof()) return std::nullopt;
	return Row(result, true);
}

std::map<LWOOBJID, std::string> SQLiteDatabase::GetLatestSnapshotHashes() {
	std::map<LWOOBJID, std::string> hashes;
	auto [_, result] = ExecuteSelect("SELECT s.character_id, s.hash FROM character_snapshots s "
		"JOIN (SELECT character_id, MAX(id) AS id FROM character_snapshots GROUP BY character_id) latest ON latest.id = s.id;");
	while (!result.eof()) {
		hashes[result.getInt64Field("character_id")] = result.getStringField("hash");
		result.nextRow();
	}
	return hashes;
}

uint32_t SQLiteDatabase::PruneCharacterSnapshots(int64_t beforeTime, uint32_t keep) {
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM character_snapshots WHERE taken_at < ? AND id IN (SELECT id FROM "
		"(SELECT id, ROW_NUMBER() OVER (PARTITION BY character_id ORDER BY id DESC) AS position FROM character_snapshots) WHERE position > ?);",
		beforeTime, keep));
}
