#pragma once
#include "CppScripts.h"

// Plays a sparkle effect on anything that touches this phantom physics object.
class ForceFieldEffect : public CppScripts::Script {
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
