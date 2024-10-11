#pragma once
#include "NjRailPostServer.h"

class NjRailActivatorsServer : public NjRailPostServer {
	void OnUse(Entity* self, Entity* user) override;

	void OnQuickBuildComplete(Entity* self, Entity* target) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};
