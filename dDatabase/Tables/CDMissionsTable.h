#pragma once

// Custom Classes
#include "CDTable.h"
#include <map>
#include <cstdint>

/*!
 \file CDMissionsTable.hpp
 \brief Contains data for the Missions table
 */

 //! Missions Struct
struct CDMissions {
	int id;                        //!< The Mission ID
	std::string defined_type;          //!< The type of mission
	std::string defined_subtype;       //!< The subtype of the mission
	int UISortOrder;               //!< The UI Sort Order for the mission
	int offer_objectID;            //!< The LOT of the mission giver
	int target_objectID;           //!< The LOT of the mission's target
	int64_t reward_currency;           //!< The amount of currency to reward the player
	int LegoScore;                 //!< The amount of LEGO Score to reward the player
	int64_t reward_reputation;         //!< The reputation to award the player
	bool isChoiceReward;            //!< Whether or not the user has the option to choose their loot
	int reward_item1;              //!< The first rewarded item
	int reward_item1_count;        //!< The count of the first item to be rewarded
	int reward_item2;              //!< The second rewarded item
	int reward_item2_count;        //!< The count of the second item to be rewarded
	int reward_item3;              //!< The third rewarded item
	int reward_item3_count;        //!< The count of the third item to be rewarded
	int reward_item4;              //!< The fourth rewarded item
	int reward_item4_count;        //!< The count of the fourth item to be rewarded
	int reward_emote;              //!< The first emote to be rewarded
	int reward_emote2;             //!< The second emote to be rewarded
	int reward_emote3;             //!< The third emote to be rewarded
	int reward_emote4;             //!< The fourth emote to be rewarded
	int reward_maximagination;     //!< The amount of max imagination to reward
	int reward_maxhealth;          //!< The amount of max health to reward
	int reward_maxinventory;       //!< The amount of max inventory to reward
	int reward_maxmodel;           //!< ???
	int reward_maxwidget;          //!< ???
	int reward_maxwallet;          //!< ???
	bool repeatable;                //!< Whether or not this mission can be repeated (for instance, is it a daily mission)
	int64_t reward_currency_repeatable;   //!< The repeatable reward
	int reward_item1_repeatable;      //!< The first rewarded item
	int reward_item1_repeat_count;    //!< The count of the first item to be rewarded
	int reward_item2_repeatable;      //!< The second rewarded item
	int reward_item2_repeat_count;    //!< The count of the second item to be rewarded
	int reward_item3_repeatable;      //!< The third rewarded item
	int reward_item3_repeat_count;    //!< The count of the third item to be rewarded
	int reward_item4_repeatable;      //!< The fourth rewarded item
	int reward_item4_repeat_count;    //!< The count of the fourth item to be rewarded
	int time_limit;                //!< The time limit of the mission
	bool isMission;                 //!< Maybe to differentiate between missions and achievements?
	int missionIconID;             //!< The mission icon ID
	std::string prereqMissionID;       //!< A '|' seperated list of prerequisite missions
	bool localize;                  //!< Whether or not to localize the mission
	bool inMOTD;                    //!< In Match of the Day(?)
	int64_t cooldownTime;              //!< The mission cooldown time
	bool isRandom;                  //!< ???
	std::string randomPool;            //!< ???
	int UIPrereqID;                //!< ???
	UNUSED(std::string gate_version);          //!< The gate version
	UNUSED(std::string HUDStates);             //!< ???
	UNUSED(int locStatus);                 //!< ???
	int reward_bankinventory;      //!< The amount of bank space this mission rewards
};

//! Missions table
class CDMissionsTable : public CDTable {
private:
	std::vector<CDMissions> entries;

public:

	//! Constructor
	CDMissionsTable(void);

	//! Destructor
	~CDMissionsTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDMissions> Query(std::function<bool(CDMissions)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	const std::vector<CDMissions>& GetEntries(void) const;

	const CDMissions* GetPtrByMissionID(uint32_t missionID) const;

	const CDMissions& GetByMissionID(uint32_t missionID, bool& found) const;

	static CDMissions Default;
};

