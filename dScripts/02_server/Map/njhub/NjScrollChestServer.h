#pragma once
#include "CppScripts.h"

class NjScrollChestServer : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;
};
