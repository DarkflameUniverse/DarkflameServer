#pragma once
#include "CDTable.h"

struct CDPropertyEntranceComponent {
	uint32_t id;
	uint32_t mapID;
	std::string propertyName;
	bool isOnProperty;
	std::string groupType;
};

class CDPropertyEntranceComponentTable : public CDTable {
public:
	//! Constructor
	CDPropertyEntranceComponentTable();

	//! Destructor
	~CDPropertyEntranceComponentTable();

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	[[nodiscard]] std::string GetName() const override;


	//! Queries the table with a custom "where" clause
	/*!
	  \param predicate The predicate
	 */
	CDPropertyEntranceComponent GetByID(uint32_t id);

	//! Gets all the entries in the table
	/*!
	  \return The entries
	 */
	[[nodiscard]] std::vector<CDPropertyEntranceComponent> GetEntries() const { return entries; }
private:
	std::vector<CDPropertyEntranceComponent> entries{};
	CDPropertyEntranceComponent defaultEntry{};
};
