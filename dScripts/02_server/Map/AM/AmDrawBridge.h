#pragma once
#include "CppScripts.h"

class AmDrawBridge : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0) override;

	void MoveBridgeDown(Entity* self, Entity* bridge, bool down);
	void NotifyDie(Entity* self, Entity* other);

	Entity* GetBridge(Entity* self);
};
