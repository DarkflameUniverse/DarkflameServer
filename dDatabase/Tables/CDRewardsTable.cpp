#include "CDRewardsTable.h"

void CDRewardsTable::LoadValuesFromDatabase() {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Rewards");
	while (!tableData.eof()) {
		CDRewards entry;
		uint32_t id = tableData.getIntField("id", -1);
		entry.levelID = tableData.getIntField("LevelID", -1);
		entry.missionID = tableData.getIntField("MissionID", -1);
		entry.rewardType = tableData.getIntField("RewardType", -1);
		entry.value = tableData.getIntField("value", -1);
		entry.count = tableData.getIntField("count", -1);

		m_entries.push_back(entry);
		tableData.nextRow();
	}

	tableData.finalize();
}

std::vector<CDRewards> CDRewardsTable::GetByLevelID(uint32_t levelID) {
	std::vector<CDRewards> result;
	for (const auto& levelData : m_entries) {
		if (levelData.levelID == levelID) result.push_back(levelData);
	}

	return result;
}

