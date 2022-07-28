#pragma once

// Custom Classes
#include "CDTable.h"

/*!
  \file CDComponentsRegistryTable.hpp
  \brief Contains data for the ComponentsRegistry table
 */

 //! ComponentsRegistry Entry Struct
struct CDComponentsRegistry {
	unsigned int id;                    //!< The LOT is used as the ID
	unsigned int component_type;        //!< See ComponentTypes enum for values
	unsigned int component_id;          //!< The ID used within the component's table (0 may either mean it's non-networked, or that the ID is actually 0
};


//! ComponentsRegistry table
class CDComponentsRegistryTable : public CDTable {
private:
	//std::vector<CDComponentsRegistry> entries;
	std::map<uint64_t, uint32_t> mappedEntries; //id, component_type, component_id

public:

	//! Constructor
	CDComponentsRegistryTable(void);

	//! Destructor
	~CDComponentsRegistryTable(void);

	//! Returns the table's name
	/*!
	  \return The table name
	 */
	std::string GetName(void) const override;

	int32_t GetByIDAndType(uint32_t id, uint32_t componentType, int32_t defaultValue = 0);
};
