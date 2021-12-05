#pragma once

// Custom Classes
#include "CDTable.h"
#include <map>

/*!
 \file CDBehaviorParameterTable.hpp
 \brief Contains data for the BehaviorParameter table
 */

//! BehaviorParameter Entry Struct
struct CDBehaviorParameter {
    unsigned int behaviorID;            //!< The Behavior ID
    std::string parameterID;       //!< The Parameter ID
    float value;                 //!< The value of the behavior template
};

//! BehaviorParameter table
class CDBehaviorParameterTable : public CDTable {
private:
    std::map<size_t, float> m_Entries;
    
public:
    
    //! Constructor
    CDBehaviorParameterTable(void);
    
    //! Destructor
    ~CDBehaviorParameterTable(void);
    
    //! Returns the table's name
    /*!
      \return The table name
     */
    std::string GetName(void) const override;
    
	float GetEntry(const uint32_t behaviorID, const std::string& name);
};
