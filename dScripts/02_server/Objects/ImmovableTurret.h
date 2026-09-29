#pragma once
#include "CppScripts.h"

// L_TURRET: turret enemies can't be stunned, interrupted, knocked back or pulled
class ImmovableTurret : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
};
