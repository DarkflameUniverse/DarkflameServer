#pragma once
#include "CppScripts.h"

class AgBugsprayer : public CppScripts::Script
{
public:
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};

