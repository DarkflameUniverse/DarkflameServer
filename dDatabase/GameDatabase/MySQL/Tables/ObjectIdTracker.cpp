#include "MySQLDatabase.h"

std::optional<uint64_t> MySQLDatabase::GetCurrentPersistentId() {
	auto result = ExecuteSelect("SELECT last_object_id FROM object_id_tracker");
	if (!result->next()) {
		return std::nullopt;
	}
	return result->getUInt64("last_object_id");
}

void MySQLDatabase::InsertDefaultPersistentId() {
	ExecuteInsert("INSERT INTO object_id_tracker VALUES (1);");
}

IObjectIdTracker::Range MySQLDatabase::GetPersistentIdRange() {
	IObjectIdTracker::Range range;
	auto prevCommit = GetAutoCommit();
	SetAutoCommit(false);

	// THIS MUST ABSOLUTELY NOT FAIL. These IDs are expected to be unique. As such a transactional select is used to safely get a range
	// of IDs that will never be used again. A separate feature could track unused IDs and recycle them, but that is not implemented.
	ExecuteCustomQuery("START TRANSACTION;");
	// 200 seems like a good range to reserve at once. Only way this would be noticable is if a player
	// added hundreds of items at once.
	// Doing the update first ensures that all other connections are blocked from accessing this table until we commit.
	auto result = ExecuteUpdate("UPDATE object_id_tracker SET last_object_id = last_object_id + 200;");
	// If no rows were updated, it means the table is empty, so we need to insert the default row first.
	if (result == 0) {
		InsertDefaultPersistentId();
		result = ExecuteUpdate("UPDATE object_id_tracker SET last_object_id = last_object_id + 200;");
	}

	// At this point all connections are waiting on us to finish the transaction, so we can safely select the ID we just set.
	auto selectRes = ExecuteSelect("SELECT last_object_id FROM object_id_tracker;");
	selectRes->next();
	range.maxID = selectRes->getUInt64("last_object_id");
	range.minID = range.maxID - 199;

	ExecuteCustomQuery("COMMIT;");
	SetAutoCommit(prevCommit);
	return range;
}
