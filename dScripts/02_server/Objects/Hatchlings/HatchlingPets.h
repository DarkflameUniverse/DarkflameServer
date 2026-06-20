#ifndef HATCHLINGPETS_H
#define HATCHLINGPETS_H

#include "CppScripts.h"
#include "NiPoint3.h"

class HatchlingPets : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void Wander(Entity& self, Entity& player);
};

#endif  //!HATCHLINGPETS_H
