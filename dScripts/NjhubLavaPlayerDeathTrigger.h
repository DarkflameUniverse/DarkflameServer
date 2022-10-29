#pragma once
#include "CppScripts.h"

class NjhubLavaPlayerDeathTrigger : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
