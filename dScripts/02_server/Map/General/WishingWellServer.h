#pragma once
#include "CppScripts.h"

class WishingWellServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};

