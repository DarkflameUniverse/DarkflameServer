#pragma once
#include "CppScripts.h"

class AmTeapotServer : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
private:
	LOT BLUE_FLOWER_LEAVES = 12317;
	LOT WU_S_IMAGINATION_TEA = 12109;
};
