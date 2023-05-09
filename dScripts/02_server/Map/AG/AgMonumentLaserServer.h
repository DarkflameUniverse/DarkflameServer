#pragma once
#include "CppScripts.h"

class AgMonumentLaserServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnDie(Entity* self, Entity* killer) override;
};
