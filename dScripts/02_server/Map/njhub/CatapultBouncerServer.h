#pragma once
#include "CppScripts.h"

class CatapultBouncerServer : public CppScripts::Script {
public:
	void OnRebuildComplete(Entity* self, Entity* target) override;
};
