#pragma once
#include "CppScripts.h"

class SsModularBuildServer : public CppScripts::Script
{
public:
	void OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules);
};
