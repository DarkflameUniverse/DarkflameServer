#pragma once
#include "CppScripts.h"

class LupGenericInteract : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
private:
	const uint32_t frakjawChestId = 16486;
};
