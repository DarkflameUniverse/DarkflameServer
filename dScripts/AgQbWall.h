#pragma once
#include "CppScripts.h"

class AgQbWall : public CppScripts::Script {
public:
	void OnRebuildComplete(Entity* self, Entity* player) override;
};
