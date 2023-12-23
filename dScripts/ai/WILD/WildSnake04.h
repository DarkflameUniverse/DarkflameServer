#pragma once
#include "CppScripts.h"

class WildSnake04 : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnDie(Entity* self, Entity* killer) override;
};
