#include "IBehaviors.h"

#include "SQLiteDatabase.h"

void SQLiteDatabase::AddBehavior(const IBehaviors::Info& info) {
	ExecuteInsert(
		"INSERT INTO behaviors (behavior_info, character_id, behavior_id) VALUES (?, ?, ?) ON CONFLICT(behavior_id) DO UPDATE SET behavior_info = ?",
		info.behaviorInfo, info.characterId, info.behaviorId, info.behaviorInfo
	);
}

void SQLiteDatabase::RemoveBehavior(const int32_t behaviorId) {
	ExecuteDelete("DELETE FROM behaviors WHERE behavior_id = ?", behaviorId);
}

std::string SQLiteDatabase::GetBehavior(const int32_t behaviorId) {
	auto [_, result] = ExecuteSelect("SELECT behavior_info FROM behaviors WHERE behavior_id = ?", behaviorId);
	return !result.eof() ? result.getStringField("behavior_info") : "";
}
