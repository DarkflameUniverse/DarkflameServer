#include "MySQLDatabase.h"

void MySQLDatabase::UpdateActivityLog(const uint32_t accountId, const eActivityType activityType, const LWOMAPID mapId) {
	ExecuteInsert("INSERT INTO activity_log (character_id, activity, time, map_id) VALUES (?, ?, ?, ?);",
		accountId, static_cast<uint32_t>(activityType), static_cast<uint32_t>(time(NULL)), mapId);
}
