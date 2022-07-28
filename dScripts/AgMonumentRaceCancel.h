#pragma once
#include "CppScripts.h"

class AgMonumentRaceCancel : public CppScripts::Script {
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
