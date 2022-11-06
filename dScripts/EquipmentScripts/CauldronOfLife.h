#pragma once
#include "ScriptedPowerupSpawner.h"

class CauldronOfLife : public ScriptedPowerupSpawner {
	void OnStartup(Entity* self) override;
};
