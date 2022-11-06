#pragma once
#include "CppScripts.h"

class FvRaceSmashEggImagineServer : public CppScripts::Script {
	void OnDie(Entity* self, Entity* killer) override;
};
