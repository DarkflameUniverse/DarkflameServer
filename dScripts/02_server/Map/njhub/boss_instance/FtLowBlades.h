#pragma once
#include "CppScripts.h"

class FtLowBlades : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

	int m_Counter;
};
