#pragma once
#include "CppScripts.h"

class AgJetEffectServer final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	
	void OnUse(Entity* self, Entity* user) override;

	void OnRebuildComplete(Entity* self, Entity* target) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	LWOOBJID builder;
	
	bool inUse;
};
