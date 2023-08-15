#pragma once
#include "CppScripts.h"

class GfOrgan : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user);
	void OnTimerDone(Entity* self, std::string timerName);
private:
	bool m_canUse;
};
