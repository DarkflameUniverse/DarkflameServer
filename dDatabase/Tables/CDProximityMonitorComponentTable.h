#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDProximityMonitorComponentTable.hpp
 \brief Contains data for the ProximityMonitorComponent table
 */

 //! ProximityMonitorComponent Entry Struct
struct CDProximityMonitorComponent {
	unsigned int id;
	std::string Proximities;
	bool LoadOnClient;
	bool LoadOnServer;
};


//! ProximityMonitorComponent table
class CDProximityMonitorComponentTable : public CDTable {
private:
	std::vector<CDProximityMonitorComponent> entries;

public:

	//! Constructor
	CDProximityMonitorComponentTable(void);

	//! Destructor
	~CDProximityMonitorComponentTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	std::vector<CDProximityMonitorComponent> Query(std::function<bool(CDProximityMonitorComponent)> predicate);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	std::vector<CDProximityMonitorComponent> GetEntries(void) const;

};
