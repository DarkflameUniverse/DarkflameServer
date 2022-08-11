#pragma once

// Custom Classes
#include "CDTable.h"
#include <unordered_map>
#include <unordered_set>

/*!
 \file CDBehaviorTemplateTable.hpp
 \brief Contains data for the BehaviorTemplate table
 */

//! BehaviorTemplate Entry Struct
struct CDBehaviorTemplate {
    unsigned int behaviorID;                                         //!< The Behavior ID
    unsigned int templateID;                                         //!< The Template ID (LOT)
    unsigned int effectID;                                           //!< The Effect ID attached
    size_t effectHandle;                                             //!< The effect handle
};


//! BehaviorTemplate table
class CDBehaviorTemplateTable : public CDTable {
public:
    void LoadHost() override;
    
    //! Constructor
    CDBehaviorTemplateTable(void);
    
    //! Destructor
    ~CDBehaviorTemplateTable(void);
    
    //! Returns the table's name
    /*!
      \return The table name
     */
    std::string GetName(void) const override;
    
    const CDBehaviorTemplate& GetByBehaviorID(const uint32_t behaviorID) const;
};
