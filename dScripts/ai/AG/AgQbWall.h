#pragma once
#include "CppScripts.h"

class AgQbWall : public CppScripts::Script {
public:
	void OnQuickBuildComplete(Entity* self, Entity* player) override;
};
