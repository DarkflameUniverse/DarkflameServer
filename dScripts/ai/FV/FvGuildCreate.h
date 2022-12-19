#pragma once
#include "CppScripts.h"

class FvGuildCreate : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
};
