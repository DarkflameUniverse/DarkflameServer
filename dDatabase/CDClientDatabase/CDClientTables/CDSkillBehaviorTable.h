#pragma once

// Custom Classes
#include "CDTable.h"

struct CDSkillBehavior {
	uint32_t skillID;               //!< The Skill ID of the skill
	UNUSED(uint32_t locStatus);             //!< ??
	uint32_t behaviorID;            //!< The Behavior ID of the skill
	uint32_t imaginationcost;       //!< The imagination cost of the skill
	uint32_t cooldowngroup;         //!< The cooldown group ID of the skill
	float cooldown;             //!< The cooldown time of the skill
	UNUSED(bool isNpcEditor);           //!< ???
	UNUSED(uint32_t skillIcon);             //!< The Skill Icon ID
	UNUSED(std::string oomSkillID);        //!< ???
	UNUSED(uint32_t oomBehaviorEffectID);   //!< ???
	UNUSED(uint32_t castTypeDesc);          //!< The cast type description(?)
	UNUSED(uint32_t imBonusUI);             //!< The imagination bonus of the skill
	UNUSED(nint32_t lifeBonusUI);           //!< The life bonus of the skill
	UNUSED(uint32_t armorBonusUI);          //!< The armor bonus of the skill
	UNUSED(uint32_t damageUI);             //!< ???
	UNUSED(bool hideIcon);              //!< Whether or not to show the icon
	UNUSED(bool localize);              //!< ???
	UNUSED(std::string gate_version);      //!< ???
	UNUSED(uint32_t cancelType);            //!< The cancel type (?)
};

class CDSkillBehaviorTable : public CDTable<CDSkillBehaviorTable> {
private:
	std::map<uint32_t, CDSkillBehavior> entries;
	CDSkillBehavior m_empty;

public:
	void LoadValuesFromDatabase();

	// Gets an entry by skillID
	const CDSkillBehavior& GetSkillByID(uint32_t skillID);
};

