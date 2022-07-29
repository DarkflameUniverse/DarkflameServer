#pragma once
#include "CppScripts.h"

class NjRailSwitch : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;
};
