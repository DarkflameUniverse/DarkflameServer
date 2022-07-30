#include "CDRewardsTable.h"

CDRewardsTable::CDRewardsTable(void) {
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Rewards");
	while (!tableData.eof()) {
		CDRewards* entry = new CDRewards();
		entry->id = tableData.getIntField(0, -1);
		entry->levelID = tableData.getIntField(1, -1);
		entry->missionID = tableData.getIntField(2, -1);
		entry->rewardType = tableData.getIntField(3, -1);
		entry->value = tableData.getIntField(4, -1);
		entry->count = tableData.getIntField(5, -1);

		m_entries.insert(std::make_pair(entry->id, entry));
		tableData.nextRow();
	}

	tableData.finalize();
}

CDRewardsTable::~CDRewardsTable(void) {
	for (auto e : m_entries) {
		if (e.second) delete e.second;
	}

	m_entries.clear();
}

std::string CDRewardsTable::GetName(void) const {
	return "Rewards";
}

std::vector<CDRewards*> CDRewardsTable::GetByLevelID(uint32_t levelID) {
	std::vector<CDRewards*> result{};
	for (const auto& e : m_entries) {
		if (e.second->levelID == levelID) result.push_back(e.second);
	}

	return result;
}
