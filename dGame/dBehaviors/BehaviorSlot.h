#pragma once

#ifndef BEHAVIORSLOT_H
#define BEHAVIORSLOT_H
#include <cstdint>

enum class BehaviorSlot : int32_t {
	Invalid = -1,
	Primary,
	Offhand,
	Neck,
	Head,
	Consumable
};

#endif
