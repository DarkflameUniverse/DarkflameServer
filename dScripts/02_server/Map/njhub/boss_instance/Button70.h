#pragma once
#include "CppScripts.h"

class Button70 : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;

	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

private:

	int IsInMovement;
};

