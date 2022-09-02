#pragma once
#include "CppScripts.h"

class QbSpawner : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnChildLoaded(Entity* self, Entity* child);
	void OnChildRemoved(Entity* self, Entity* child);
	void AggroTargetObject(Entity* self, Entity* enemy);
private:
	const int m_DefaultMobNum = 3;
	const float m_DefaultSpawnDist = 25.0;
	const LWOOBJID m_DefaultMobTemplate = 4712;
	const float m_DefaultSpawnTime = 2.0;
};
