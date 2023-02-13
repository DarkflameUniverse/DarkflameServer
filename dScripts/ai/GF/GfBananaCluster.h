#pragma once
#include "CppScripts.h"

class GfBananaCluster final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};
