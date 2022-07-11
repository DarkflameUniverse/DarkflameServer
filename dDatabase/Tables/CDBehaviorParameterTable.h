#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

/*!
 \file CDBehaviorParameterTable.hpp
 \brief Contains data for the BehaviorParameter table
 */

//! BehaviorParameter Entry Struct
struct CDBehaviorParameter {
    int32_t behaviorID;            //!< The Behavior ID
    int32_t parameterID;       //!< The Parameter ID
    float value;                 //!< The value of the behavior template
};

//! BehaviorParameter table
class CDBehaviorParameterTable : public CDTable {
private:
	std::unordered_map<size_t, float> m_Entries;
public:
    static void CreateSharedMap();
    
    //! Constructor
    CDBehaviorParameterTable(void);
    
    //! Destructor
    ~CDBehaviorParameterTable(void);
    
    //! Returns the table's name
    /*!
      \return The table name
     */
    std::string GetName(void) const override;
    
	float GetEntry(const uint32_t behaviorID, const std::string& name, const float defaultValue = 0);
};
