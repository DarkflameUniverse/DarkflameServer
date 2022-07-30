#pragma once
#include "CppScripts.h"

class NsQbImaginationStatue : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void SpawnLoot(Entity* self);
};
