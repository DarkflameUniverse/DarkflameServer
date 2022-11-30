#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDMissionEmailTable.hpp
 \brief Contains data for the MissionEmail table
 */

 //! MissionEmail Entry Struct
struct CDMissionEmail {
	unsigned int ID;
	unsigned int messageType;
	unsigned int notificationGroup;
	unsigned int missionID;
	unsigned int attachmentLOT;
	bool localize;
	unsigned int locStatus;
	std::string gate_version;
};


//! MissionEmail table
class CDMissionEmailTable : public CDTable {
private:
	std::vector<CDMissionEmail> entries;

public:

	//! Constructor
	CDMissionEmailTable(void);

	//! Destructor
	~CDMissionEmailTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDMissionEmail> Query(std::function<bool(CDMissionEmail)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDMissionEmail> GetEntries(void) const;

};
