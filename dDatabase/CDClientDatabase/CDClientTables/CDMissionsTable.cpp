#include "CDMissionsTable.h"

CDMissions CDMissionsTable::Default = {};

void CDMissionsTable::LoadValuesFromDatabase() {

	// First, get the size of the table
	unsigned int size = 0;
	auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM Missions");
	while (!tableSize.eof()) {
		size = tableSize.getIntField(0, 0);

		tableSize.nextRow();
	}

	tableSize.finalize();

	// Reserve the size
	m_Entries.reserve(size);

	// Now get the data
	auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Missions");
	while (!tableData.eof()) {
		auto& entry = m_Entries.emplace_back();
		entry.id = tableData.getIntField("id", -1);
		entry.defined_type = tableData.getStringField("defined_type", "");
		entry.defined_subtype = tableData.getStringField("defined_subtype", "");
		entry.UISortOrder = tableData.getIntField("UISortOrder", -1);
		entry.offer_objectID = tableData.getIntField("offer_objectID", -1);
		entry.target_objectID = tableData.getIntField("target_objectID", -1);
		entry.reward_currency = tableData.getInt64Field("reward_currency", -1);
		entry.LegoScore = tableData.getIntField("LegoScore", -1);
		entry.reward_reputation = tableData.getIntField("reward_reputation", -1);
		entry.isChoiceReward = tableData.getIntField("isChoiceReward", -1) == 1 ? true : false;
		entry.reward_item1 = tableData.getIntField("reward_item1", 0);
		entry.reward_item1_count = tableData.getIntField("reward_item1_count", 0);
		entry.reward_item2 = tableData.getIntField("reward_item2", 0);
		entry.reward_item2_count = tableData.getIntField("reward_item2_count", 0);
		entry.reward_item3 = tableData.getIntField("reward_item3", 0);
		entry.reward_item3_count = tableData.getIntField("reward_item3_count", 0);
		entry.reward_item4 = tableData.getIntField("reward_item4", 0);
		entry.reward_item4_count = tableData.getIntField("reward_item4_count", 0);
		entry.reward_emote = tableData.getIntField("reward_emote", -1);
		entry.reward_emote2 = tableData.getIntField("reward_emote2", -1);
		entry.reward_emote3 = tableData.getIntField("reward_emote3", -1);
		entry.reward_emote4 = tableData.getIntField("reward_emote4", -1);
		entry.reward_maximagination = tableData.getIntField("reward_maximagination", -1);
		entry.reward_maxhealth = tableData.getIntField("reward_maxhealth", -1);
		entry.reward_maxinventory = tableData.getIntField("reward_maxinventory", -1);
		entry.reward_maxmodel = tableData.getIntField("reward_maxmodel", -1);
		entry.reward_maxwidget = tableData.getIntField("reward_maxwidget", -1);
		entry.reward_maxwallet = tableData.getIntField("reward_maxwallet", -1);
		entry.repeatable = tableData.getIntField("repeatable", -1) == 1 ? true : false;
		entry.reward_currency_repeatable = tableData.getIntField("reward_currency_repeatable", -1);
		entry.reward_item1_repeatable = tableData.getIntField("reward_item1_repeatable", -1);
		entry.reward_item1_repeat_count = tableData.getIntField("reward_item1_repeat_count", -1);
		entry.reward_item2_repeatable = tableData.getIntField("reward_item2_repeatable", -1);
		entry.reward_item2_repeat_count = tableData.getIntField("reward_item2_repeat_count", -1);
		entry.reward_item3_repeatable = tableData.getIntField("reward_item3_repeatable", -1);
		entry.reward_item3_repeat_count = tableData.getIntField("reward_item3_repeat_count", -1);
		entry.reward_item4_repeatable = tableData.getIntField("reward_item4_repeatable", -1);
		entry.reward_item4_repeat_count = tableData.getIntField("reward_item4_repeat_count", -1);
		entry.time_limit = tableData.getIntField("time_limit", -1);
		entry.isMission = tableData.getIntField("isMission", -1) ? true : false;
		entry.missionIconID = tableData.getIntField("missionIconID", -1);
		entry.prereqMissionID = tableData.getStringField("prereqMissionID", "");
		entry.localize = tableData.getIntField("localize", -1) == 1 ? true : false;
		entry.inMOTD = tableData.getIntField("inMOTD", -1) == 1 ? true : false;
		entry.cooldownTime = tableData.getInt64Field("cooldownTime", -1);
		entry.isRandom = tableData.getIntField("isRandom", -1) == 1 ? true : false;
		entry.randomPool = tableData.getStringField("randomPool", "");
		entry.UIPrereqID = tableData.getIntField("UIPrereqID", -1);
		UNUSED(entry.gate_version = tableData.getStringField("gate_version", ""));
		UNUSED(entry.HUDStates = tableData.getStringField("HUDStates", ""));
		UNUSED(entry.locStatus = tableData.getIntField("locStatus", -1));
		entry.reward_bankinventory = tableData.getIntField("reward_bankinventory", -1);

		tableData.nextRow();
	}

	Default.id = -1;
}

std::vector<CDMissions> CDMissionsTable::Query(std::function<bool(CDMissions)> predicate) {

	std::vector<CDMissions> data = cpplinq::from(m_Entries)
		>> cpplinq::where(predicate)
		>> cpplinq::to_vector();

	return data;
}

const std::vector<CDMissions>& CDMissionsTable::GetEntries(void) const {
	return m_Entries;
}

const CDMissions* CDMissionsTable::GetPtrByMissionID(const uint32_t missionID) const {
	for (const auto& entry : m_Entries) {
		if (entry.id == missionID) {
			return const_cast<CDMissions*>(&entry);
		}
	}

	return &Default;
}

const CDMissions& CDMissionsTable::GetByMissionID(const uint32_t missionID, bool& found) const {
	for (const auto& entry : m_Entries) {
		if (entry.id == missionID) {
			found = true;

			return entry;
		}
	}

	found = false;

	return Default;
}
