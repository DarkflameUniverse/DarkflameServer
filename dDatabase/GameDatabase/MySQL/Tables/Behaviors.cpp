#include "IBehaviors.h"

#include "MySQLDatabase.h"

void MySQLDatabase::AddBehavior(const IBehaviors::Info& info) {
	ExecuteInsert(
		"INSERT INTO behaviors (behavior_info, character_id, behavior_id) VALUES (?, ?, ?) ON DUPLICATE KEY UPDATE behavior_info = ?",
		info.behaviorInfo, info.characterId, info.behaviorId, info.behaviorInfo
	);
}

void MySQLDatabase::RemoveBehavior(const int32_t behaviorId) {
	ExecuteDelete("DELETE FROM behaviors WHERE behavior_id = ?", behaviorId);
}

std::string MySQLDatabase::GetBehavior(const int32_t behaviorId) {
	auto result = ExecuteSelect("SELECT behavior_info FROM behaviors WHERE behavior_id = ?", behaviorId);
	return result->next() ? result->getString("behavior_info").c_str() : "";
}
