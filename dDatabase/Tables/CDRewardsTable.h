#pragma once
#include "CDTable.h"
#include <string>

struct CDRewards {
	int32_t levelID;
	int32_t missionID;
	int32_t rewardType;
	int32_t value;
	int32_t count;
};

class CDRewardsTable : public CDTable<CDRewardsTable> {
public:
	void LoadValuesFromDatabase();
	std::vector<CDRewards> GetByLevelID(uint32_t levelID);
private:
	std::vector<CDRewards> m_entries;
};
