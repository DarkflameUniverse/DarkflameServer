#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDActivitiesTable.hpp
 \brief Contains data for the Activities table
 */

 //! Activities Entry Struct
struct CDActivities {
	unsigned int ActivityID;
	unsigned int locStatus;
	unsigned int instanceMapID;
	unsigned int minTeams;
	unsigned int maxTeams;
	unsigned int minTeamSize;
	unsigned int maxTeamSize;
	unsigned int waitTime;
	unsigned int startDelay;
	bool requiresUniqueData;
	unsigned int leaderboardType;
	bool localize;
	int optionalCostLOT;
	int optionalCostCount;
	bool showUIRewards;
	unsigned int CommunityActivityFlagID;
	std::string gate_version;
	bool noTeamLootOnDeath;
	float optionalPercentage;
};


//! Activities table
class CDActivitiesTable : public CDTable {
private:
	std::vector<CDActivities> entries;

public:

	//! Constructor
	CDActivitiesTable(void);

	//! Destructor
	~CDActivitiesTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDActivities> Query(std::function<bool(CDActivities)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDActivities> GetEntries(void) const;

};
