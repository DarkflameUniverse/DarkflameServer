#pragma once
#include "CppScripts.h"

class FtFlameJetServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnCollisionPhantom(Entity* self, Entity* target) override;

	void SpawnLegs(Entity* self);

	void OnChildLoaded(Entity* self, Entity* child);

	void NotifyDie(Entity* self, Entity* other, Entity* killer);

	void OnChildRemoved(Entity* self, Entity* child);

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

	int SharedInt;

};
