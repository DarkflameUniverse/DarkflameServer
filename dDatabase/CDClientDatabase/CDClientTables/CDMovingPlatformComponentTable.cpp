#include "CDMovingPlatformComponentTable.h"

CDMovingPlatformComponentTable::CDMovingPlatformComponentTable() {
	auto& entries = GetEntriesMutable();
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM MovingPlatforms");
	while (!tableData.eof()) {
		CDMovingPlatformTableEntry entry;
		entry.platformIsSimpleMover = tableData.getIntField("platformIsSimpleMover", 0) == 1;
		entry.platformStartAtEnd = tableData.getIntField("platformStartAtEnd", 0) == 1;
		entry.platformMove.x = tableData.getFloatField("platformMoveX", 0.0f);
		entry.platformMove.y = tableData.getFloatField("platformMoveY", 0.0f);
		entry.platformMove.z = tableData.getFloatField("platformMoveZ", 0.0f);
		entry.moveTime = tableData.getFloatField("platformMoveTime", -1.0f);

		DluAssert(entries.insert(std::make_pair(tableData.getIntField("id", -1), entry)).second);
		tableData.nextRow();
	}
}

void CDMovingPlatformComponentTable::CachePlatformEntry(ComponentID id) {
	auto& entries = GetEntriesMutable();
	auto query = CDClientDatabase::CreatePreppedStmt("SELECT * FROM MovingPlatforms WHERE id = ?;");
	query.bind(1, static_cast<int32_t>(id));

	auto tableData = query.execQuery();
	while (!tableData.eof()) {
		CDMovingPlatformTableEntry entry;
		entry.platformIsSimpleMover = tableData.getIntField("platformIsSimpleMover", 0) == 1;
		entry.platformStartAtEnd = tableData.getIntField("platformStartAtEnd", 0) == 1;
		entry.platformMove.x = tableData.getFloatField("platformMoveX", 0.0f);
		entry.platformMove.y = tableData.getFloatField("platformMoveY", 0.0f);
		entry.platformMove.z = tableData.getFloatField("platformMoveZ", 0.0f);
		entry.moveTime = tableData.getFloatField("platformMoveTime", -1.0f);

		DluAssert(entries.insert(std::make_pair(tableData.getIntField("id", -1), entry)).second);
		tableData.nextRow();
	}
}

const std::optional<CDMovingPlatformTableEntry> CDMovingPlatformComponentTable::GetPlatformEntry(ComponentID id) {
	auto& entries = GetEntriesMutable();
	auto itr = entries.find(id);
	if (itr == entries.end()) {
		CachePlatformEntry(id);
		itr = entries.find(id);
	}
	return itr != entries.end() ? std::make_optional<CDMovingPlatformTableEntry>(itr->second) : std::nullopt;
}
