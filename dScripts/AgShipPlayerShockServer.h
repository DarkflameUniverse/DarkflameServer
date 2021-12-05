#pragma once
#include "CppScripts.h"

class AgShipPlayerShockServer : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user);
	void OnTimerDone(Entity* self, std::string timerName);
private:
	std::u16string shockAnim = u"knockback-recovery";
	float fxTime = 2.0;
	bool active = false;
};

