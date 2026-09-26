#include "MySQLDatabase.h"

#include <sstream>

namespace {
	ICharacterSnapshots::CharacterSnapshot Row(PreparedStmtResultSet& result, bool withXml) {
		ICharacterSnapshots::CharacterSnapshot snapshot;
		snapshot.id = result->getUInt64("id");
		snapshot.characterId = result->getInt64("character_id");
		snapshot.takenAt = result->getInt64("taken_at");
		snapshot.reason = result->getString("reason").c_str();
		snapshot.actor = result->getString("actor").c_str();
		snapshot.size = result->getUInt("size");
		snapshot.hash = result->getString("hash").c_str();
		if (withXml) {
			std::unique_ptr<std::istream> blob(result->getBlob("xml"));
			std::stringstream data;
			data << blob->rdbuf();
			snapshot.compressed = data.str();
		}
		return snapshot;
	}
}

void MySQLDatabase::InsertCharacterSnapshot(const CharacterSnapshot& snapshot) {
	std::istringstream xml(snapshot.compressed);
	ExecuteInsert("INSERT INTO character_snapshots (character_id, taken_at, reason, actor, size, hash, xml) VALUES (?, ?, ?, ?, ?, ?, ?);",
		snapshot.characterId, snapshot.takenAt, snapshot.reason, snapshot.actor, snapshot.size, snapshot.hash, static_cast<const std::istream*>(&xml));
}

std::vector<ICharacterSnapshots::CharacterSnapshot> MySQLDatabase::GetCharacterSnapshots(LWOOBJID characterId) {
	std::vector<CharacterSnapshot> snapshots;
	auto result = ExecuteSelect("SELECT id, character_id, taken_at, reason, actor, size, hash FROM character_snapshots WHERE character_id = ? ORDER BY id DESC;", characterId);
	while (result->next()) snapshots.push_back(Row(result, false));
	return snapshots;
}

std::optional<ICharacterSnapshots::CharacterSnapshot> MySQLDatabase::GetCharacterSnapshot(uint64_t id) {
	auto result = ExecuteSelect("SELECT * FROM character_snapshots WHERE id = ?;", static_cast<int64_t>(id));
	if (!result->next()) return std::nullopt;
	return Row(result, true);
}

std::map<LWOOBJID, std::string> MySQLDatabase::GetLatestSnapshotHashes() {
	std::map<LWOOBJID, std::string> hashes;
	auto result = ExecuteSelect("SELECT s.character_id, s.hash FROM character_snapshots s "
		"JOIN (SELECT character_id, MAX(id) AS id FROM character_snapshots GROUP BY character_id) latest ON latest.id = s.id;");
	while (result->next()) hashes[result->getInt64("character_id")] = result->getString("hash").c_str();
	return hashes;
}

uint32_t MySQLDatabase::PruneCharacterSnapshots(int64_t beforeTime, uint32_t keep) {
	// The extra derived table lets MySQL delete from the table it reads
	return static_cast<uint32_t>(ExecuteUpdate("DELETE FROM character_snapshots WHERE taken_at < ? AND id IN (SELECT id FROM "
		"(SELECT id, ROW_NUMBER() OVER (PARTITION BY character_id ORDER BY id DESC) AS position FROM character_snapshots) ranked WHERE position > ?);",
		beforeTime, keep));
}
