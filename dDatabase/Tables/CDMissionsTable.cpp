#include "CDMissionsTable.h"

CDMissions CDMissionsTable::Default = {};

//! Constructor
CDMissionsTable::CDMissionsTable(void) {
    
    // First, get the size of the table
    unsigned int size = 0;
    auto tableSize = CDClientDatabase::ExecuteQuery("SELECT COUNT(*) FROM Missions");
    while (!tableSize.eof()) {
        size = tableSize.getIntField(0, 0);
        
        tableSize.nextRow();
    }
        
	tableSize.finalize();
    
    // Reserve the size
    this->entries.reserve(size);
    
    // Now get the data
    auto tableData = CDClientDatabase::ExecuteQuery("SELECT * FROM Missions");
    while (!tableData.eof()) {
        CDMissions entry;
        entry.id = tableData.getIntField(0, -1);
        entry.defined_type = tableData.getStringField(1, "");
        entry.defined_subtype = tableData.getStringField(2, "");
        entry.UISortOrder = tableData.getIntField(3, -1);
        entry.offer_objectID = tableData.getIntField(4, -1);
        entry.target_objectID = tableData.getIntField(5, -1);
        entry.reward_currency = tableData.getInt64Field(6, -1);
        entry.LegoScore = tableData.getIntField(7, -1);
        entry.reward_reputation = tableData.getIntField(8, -1);
        entry.isChoiceReward = tableData.getIntField(9, -1) == 1 ? true : false;
        entry.reward_item1 = tableData.getIntField(10, 0);
        entry.reward_item1_count = tableData.getIntField(11, 0);
        entry.reward_item2 = tableData.getIntField(12, 0);
        entry.reward_item2_count = tableData.getIntField(13, 0);
        entry.reward_item3 = tableData.getIntField(14, 0);
        entry.reward_item3_count = tableData.getIntField(15, 0);
        entry.reward_item4 = tableData.getIntField(16, 0);
        entry.reward_item4_count = tableData.getIntField(17, 0);
        entry.reward_emote = tableData.getIntField(18, -1);
        entry.reward_emote2 = tableData.getIntField(19, -1);
        entry.reward_emote3 = tableData.getIntField(20, -1);
        entry.reward_emote4 = tableData.getIntField(21, -1);
        entry.reward_maximagination = tableData.getIntField(22, -1);
        entry.reward_maxhealth = tableData.getIntField(23, -1);
        entry.reward_maxinventory = tableData.getIntField(24, -1);
        entry.reward_maxmodel = tableData.getIntField(25, -1);
        entry.reward_maxwidget = tableData.getIntField(26, -1);
        entry.reward_maxwallet = tableData.getIntField(27, -1);
        entry.repeatable = tableData.getIntField(28, -1) == 1 ? true : false;
        entry.reward_currency_repeatable = tableData.getIntField(29, -1);
        entry.reward_item1_repeatable = tableData.getIntField(30, -1);
        entry.reward_item1_repeat_count = tableData.getIntField(31, -1);
        entry.reward_item2_repeatable = tableData.getIntField(32, -1);
        entry.reward_item2_repeat_count = tableData.getIntField(33, -1);
        entry.reward_item3_repeatable = tableData.getIntField(34, -1);
        entry.reward_item3_repeat_count = tableData.getIntField(35, -1);
        entry.reward_item4_repeatable = tableData.getIntField(36, -1);
        entry.reward_item4_repeat_count = tableData.getIntField(37, -1);
        entry.time_limit = tableData.getIntField(38, -1);
        entry.isMission = tableData.getIntField(39, -1) ? true : false;
        entry.missionIconID = tableData.getIntField(40, -1);
        entry.prereqMissionID = tableData.getStringField(41, "");
        entry.localize = tableData.getIntField(42, -1) == 1 ? true : false;
        entry.inMOTD = tableData.getIntField(43, -1) == 1 ? true : false;
        entry.cooldownTime = tableData.getInt64Field(44, -1);
        entry.isRandom = tableData.getIntField(45, -1) == 1 ? true : false;
        entry.randomPool = tableData.getStringField(46, "");
        entry.UIPrereqID = tableData.getIntField(47, -1);
        UNUSED(entry.gate_version = tableData.getStringField(48, ""));
        UNUSED(entry.HUDStates = tableData.getStringField(49, ""));
        UNUSED(entry.locStatus = tableData.getIntField(50, -1));
        entry.reward_bankinventory = tableData.getIntField(51, -1);
        
        this->entries.push_back(entry);
        tableData.nextRow();
    }

	tableData.finalize();

    Default.id = -1;
}

//! Destructor
CDMissionsTable::~CDMissionsTable(void) { }

//! Returns the table's name
std::string CDMissionsTable::GetName(void) const {
    return "Missions";
}

//! Queries the table with a custom "where" clause
std::vector<CDMissions> CDMissionsTable::Query(std::function<bool(CDMissions)> predicate) {
    
    std::vector<CDMissions> data = cpplinq::from(this->entries)
    >> cpplinq::where(predicate)
    >> cpplinq::to_vector();
    
    return data;
}

//! Gets all the entries in the table
const std::vector<CDMissions>& CDMissionsTable::GetEntries(void) const {
    return this->entries;
}

const CDMissions* CDMissionsTable::GetPtrByMissionID(uint32_t missionID) const
{
    for (const auto& entry : entries)
    {
        if (entry.id == missionID)
        {
            return const_cast<CDMissions*>(&entry);
        }
    }

    return &Default;
}

const CDMissions& CDMissionsTable::GetByMissionID(uint32_t missionID, bool& found) const
{
    for (const auto& entry : entries)
    {
        if (entry.id == missionID)
        {
            found = true;

            return entry;
        }
    }

    found = false;

    return Default;
}
