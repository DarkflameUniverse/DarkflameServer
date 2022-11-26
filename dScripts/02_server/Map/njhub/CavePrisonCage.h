#pragma once
#include "CppScripts.h"

class CavePrisonCage : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void Setup(Entity* self, Spawner* spawner);
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
	void SpawnCounterweight(Entity* self, Spawner* spawner);
	void GetButton(Entity* self);
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	std::vector<LOT> m_Villagers = { 15851, 15866, 15922, 15924, 15927, 15929 };
	std::vector<int32_t> m_Missions = { 1801, 2039 };
};
