#pragma once
#include "NjRailPostServer.h"

class NjRailActivatorsServer : public NjRailPostServer {
	void OnUse(Entity* self, Entity* user) override;
};
