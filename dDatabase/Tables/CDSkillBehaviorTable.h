#pragma once

// Custom Classes
#include "CDTable.h"

/*!
 \file CDSkillBehaviorTable.hpp
 \brief Contains data for the SkillBehavior table
 */

 //! ZoneTable Struct
struct CDSkillBehavior {
	unsigned int skillID;               //!< The Skill ID of the skill
	UNUSED(unsigned int locStatus);             //!< ??
	unsigned int behaviorID;            //!< The Behavior ID of the skill
	unsigned int imaginationcost;       //!< The imagination cost of the skill
	unsigned int cooldowngroup;         //!< The cooldown group ID of the skill
	float cooldown;             //!< The cooldown time of the skill
	UNUSED(bool isNpcEditor);           //!< ???
	UNUSED(unsigned int skillIcon);             //!< The Skill Icon ID
	UNUSED(std::string oomSkillID);        //!< ???
	UNUSED(unsigned int oomBehaviorEffectID);   //!< ???
	UNUSED(unsigned int castTypeDesc);          //!< The cast type description(?)
	UNUSED(unsigned int imBonusUI);             //!< The imagination bonus of the skill
	UNUSED(nsigned int lifeBonusUI);           //!< The life bonus of the skill
	UNUSED(unsigned int armorBonusUI);          //!< The armor bonus of the skill
	UNUSED(unsigned int damageUI);             //!< ???
	UNUSED(bool hideIcon);              //!< Whether or not to show the icon
	UNUSED(bool localize);              //!< ???
	UNUSED(std::string gate_version);      //!< ???
	UNUSED(unsigned int cancelType);            //!< The cancel type (?)
};

//! SkillBehavior table
class CDSkillBehaviorTable : public CDTable {
private:
	std::map<unsigned int, CDSkillBehavior> entries;
	CDSkillBehavior m_empty;

public:

	//! Constructor
	CDSkillBehaviorTable(void);

	//! Destructor
	~CDSkillBehaviorTable(void);

	//! Returns the table's name
	/*!
	 \return The table name
	 */
	std::string GetName(void) const override;

	//! Queries the table with a custom "where" clause
	/*!
	 \param predicate The predicate
	 */
	std::vector<CDSkillBehavior> Query(std::function<bool(CDSkillBehavior)> predicate);

	//! Gets an entry by ID
	const CDSkillBehavior& GetSkillByID(unsigned int skillID);
};

