#pragma once
#include "CppScripts.h"

class NsModularBuild : public CppScripts::Script
{
public:
	void OnModularBuildExit(Entity* self, Entity* player, bool bCompleted, std::vector<LOT> modules);
private:
	int m_MissionNum = 809;
};
