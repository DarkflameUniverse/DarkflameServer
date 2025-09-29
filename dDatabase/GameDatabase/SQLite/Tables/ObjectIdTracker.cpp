#include "SQLiteDatabase.h"

std::optional<uint64_t> SQLiteDatabase::GetCurrentPersistentId() {
	auto [_, result] = ExecuteSelect("SELECT last_object_id FROM object_id_tracker");
	if (result.eof()) {
		return std::nullopt;
	}
	return result.getInt64Field("last_object_id");
}

void SQLiteDatabase::InsertDefaultPersistentId() {
	ExecuteInsert("INSERT INTO object_id_tracker VALUES (1);");
}

IObjectIdTracker::Range SQLiteDatabase::GetPersistentIdRange() {
	IObjectIdTracker::Range range;
	auto prevCommit = GetAutoCommit();
	SetAutoCommit(false); // This begins the transaction for us if one is not already in progress

	// THIS MUST ABSOLUTELY NOT FAIL. These IDs are expected to be unique. As such a transactional select is used to safely get a range
	// of IDs that will never be used again. A separate feature could track unused IDs and recycle them, but that is not implemented.
	// 200 seems like a good range to reserve at once. Only way this would be noticable is if a player
	// added hundreds of items at once.
	// Doing the update first ensures that all other connections are blocked from accessing this table until we commit.
	auto result = ExecuteUpdate("UPDATE object_id_tracker SET last_object_id = last_object_id + 200;");
	if (result == 0) {
		InsertDefaultPersistentId();
		result = ExecuteUpdate("UPDATE object_id_tracker SET last_object_id = last_object_id + 200;");
	}

	// At this point all connections are waiting on us to finish the transaction, so we can safely select the ID we just set.
	auto [_, selectResult] = ExecuteSelect("SELECT last_object_id FROM object_id_tracker;");
	range.maxID = selectResult.getInt64Field("last_object_id");
	range.minID = range.maxID - 199;

	// We must commit here manually, this will unlock the database for all other servers
	ExecuteCustomQuery("COMMIT;");
	SetAutoCommit(prevCommit);
	return range;
}
