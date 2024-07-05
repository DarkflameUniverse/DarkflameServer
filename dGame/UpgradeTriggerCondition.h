#pragma once

#include <cstdint>

namespace nejlika
{

enum class UpgradeTriggerCondition
{
	None,
	Unarmed,
	Melee,
	TwoHanded,
	Shield,
	PrimaryAbility,
	UseSkill
};

}