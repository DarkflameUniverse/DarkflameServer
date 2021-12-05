#pragma once
#include "CppScripts.h"

class AgBusDoor : public CppScripts::Script
{
public:
	void OnStartup(Entity* self);
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status);
	void OnTimerDone(Entity* self, std::string timerName);
private:
	void MoveDoor(Entity* self, bool bOpen);
	int m_Counter;
	int m_OuterCounter;
};

