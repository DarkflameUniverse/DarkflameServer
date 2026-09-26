#pragma once
#include "CountdownDestroyAI.h"

class DragonRonin : public CountdownDestroyAI {
public:
	void OnStartup(Entity* self) override;
};
