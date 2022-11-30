#pragma once
#include "CppScripts.h"

class MastTeleport : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
