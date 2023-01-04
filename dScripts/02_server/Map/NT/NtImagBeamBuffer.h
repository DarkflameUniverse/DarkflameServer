#pragma once
#include "CppScripts.h"

class NtImagBeamBuffer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	std::vector<LWOOBJID> m_EntitiesInProximity = {};
};
