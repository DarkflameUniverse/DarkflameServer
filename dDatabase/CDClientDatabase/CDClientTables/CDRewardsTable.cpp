#include "CDRewardsTable.h"

void CDRewardsTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Rewards");
	auto& entries = GetEntriesMutable();
	while (!tableData.eof()) {
		CDRewards entry;
		entry.id = tableData.getIntField("id", -1);
		entry.levelID = tableData.getIntField("LevelID", -1);
		entry.missionID = tableData.getIntField("MissionID", -1);
		entry.rewardType = tableData.getIntField("RewardType", -1);
		entry.value = tableData.getIntField("value", -1);
		entry.count = tableData.getIntField("count", -1);

		entries.insert(std::make_pair(entry.id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDRewards> CDRewardsTable::GetByLevelID(uint32_t levelID) {
	std::vector<CDRewards> result{};
	for (const auto& e : GetEntries()) {
		if (e.second.levelID == levelID) result.push_back(e.second);
	}

	return result;
}

