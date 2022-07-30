#pragma once
#include "CppScripts.h"

class NjGarmadonCelebration : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
private:
	const int32_t GarmadonCelebrationID = 23;
};
