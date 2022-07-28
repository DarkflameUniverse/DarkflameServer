#pragma once
#include "CppScripts.h"

class PropertyFXDamage : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
