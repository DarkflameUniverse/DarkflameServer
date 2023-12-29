#pragma once
#include "CppScripts.h"

class CatapultBouncerServer : public CppScripts::Script {
public:
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
};
