#pragma once

// Custom Classes
#include "CDTable.h"

struct CDSkillBehavior {
	unsigned int skillID;               //!< The Skill ID of the skill
	UNUSED_COLUMN(unsigned int locStatus);             //!< ??
	unsigned int behaviorID;            //!< The Behavior ID of the skill
	unsigned int imaginationcost;       //!< The imagination cost of the skill
	unsigned int cooldowngroup;         //!< The cooldown group ID of the skill
	float cooldown;             //!< The cooldown time of the skill
	UNUSED_COLUMN(bool isNpcEditor);           //!< ???
	UNUSED_COLUMN(unsigned int skillIcon);             //!< The Skill Icon ID
	UNUSED_COLUMN(std::string oomSkillID);        //!< ???
	UNUSED_COLUMN(unsigned int oomBehaviorEffectID);   //!< ???
	UNUSED_COLUMN(unsigned int castTypeDesc);          //!< The cast type description(?)
	UNUSED_COLUMN(unsigned int imBonusUI);             //!< The imagination bonus of the skill
	UNUSED_COLUMN(nsigned int lifeBonusUI);           //!< The life bonus of the skill
	UNUSED_COLUMN(unsigned int armorBonusUI);          //!< The armor bonus of the skill
	UNUSED_COLUMN(unsigned int damageUI);             //!< ???
	UNUSED_COLUMN(bool hideIcon);              //!< Whether or not to show the icon
	UNUSED_COLUMN(bool localize);              //!< ???
	UNUSED_COLUMN(std::string gate_version);      //!< ???
	UNUSED_COLUMN(unsigned int cancelType);            //!< The cancel type (?)
};

class CDSkillBehaviorTable : public CDTable<CDSkillBehaviorTable> {
private:
	std::map<unsigned int, CDSkillBehavior> entries;
	CDSkillBehavior m_empty;

public:
	CDSkillBehaviorTable();
	// Queries the table with a custom "where" clause
	std::vector<CDSkillBehavior> Query(std::function<bool(CDSkillBehavior)> predicate);

	// Gets an entry by skillID
	const CDSkillBehavior& GetSkillByID(unsigned int skillID);
};

