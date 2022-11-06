#pragma once
#include "CppScripts.h"

class FvPandaSpawnerServer : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
