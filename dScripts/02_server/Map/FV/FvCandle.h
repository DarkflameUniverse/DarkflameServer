#pragma once
#include "CppScripts.h"

class FvCandle : public CppScripts::Script
{
public:
	void OnStartup(Entity* self);
	void OnHit(Entity* self, Entity* attacker);
	void OnTimerDone(Entity* self, std::string timerName);
private:
	void BlowOutCandle(Entity* self, Entity* blower);

	static std::vector<int32_t> m_Missions;
};
