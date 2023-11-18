#include "MySQLDatabase.h"

void MySQLDatabase::InsertCheatDetection(const IPlayerCheatDetections::Info& info) {
	ExecuteInsert(
		"INSERT INTO player_cheat_detections (account_id, name, violation_msg, violation_system_address) VALUES (?, ?, ?, ?)",
		info.userId, info.username, info.extraMessage, info.systemAddress);
}
