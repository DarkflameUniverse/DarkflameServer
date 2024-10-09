#pragma once
#include "NjRailPostServer.h"

class FtBossActivators : public NjRailPostServer {
	void OnUse(Entity* self, Entity* user) override;

	void PortalHitFX(Entity* self);

	void OnQuickBuildComplete(Entity* self, Entity* target) override;

	void OnTimerDone(Entity* self, std::string timerName) override;
};
