#pragma once
#include "CppScripts.h"

class PropertyPlatform : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
private:
	float_t movementDelay = 10.0f;
	float_t effectDelay = 5.0f;
	float_t dieDelay = 5.0f;
	uint32_t desiredWaypoint = 1;
};
