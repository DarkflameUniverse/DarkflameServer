#pragma once
#include "CppScripts.h"

class WildAndScared : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
};
