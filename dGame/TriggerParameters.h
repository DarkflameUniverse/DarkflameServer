#pragma once

#include <cstdint>
#include "BehaviorSlot.h"

namespace nejlika
{

class TriggerParameters
{
public:
	int32_t SkillID = 0;
	BehaviorSlot SelectedBehaviorSlot = BehaviorSlot::Invalid;
};

}
