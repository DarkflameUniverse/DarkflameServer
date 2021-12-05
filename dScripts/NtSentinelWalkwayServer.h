#pragma once
#include "CppScripts.h"

class NtSentinelWalkwayServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
private:
	const std::vector<int32_t> m_MissionsToUpdate = { 1047, 1330, 1331, 1332 };
};
