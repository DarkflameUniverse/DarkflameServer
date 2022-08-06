#pragma once

#include "Behavior.h"

class EmptyBehavior final : public Behavior
{
public:
	explicit EmptyBehavior(const uint32_t behavior_id) : Behavior(behavior_id) {
	}
};
