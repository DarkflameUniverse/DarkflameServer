#pragma once
#include "CppScripts.h"

class AgMonumentBirds : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status);
	void OnTimerDone(Entity* self, std::string timerName);

private:
	std::u16string sOnProximityAnim = u"fly1";
	float flyRadius = 5.0f;
};
