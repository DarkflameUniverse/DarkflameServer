#pragma once
#include "CppScripts.h"
#include "RenderComponent.h"

class FvHorsemenTrigger : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;

private:
	std::vector<int32_t> m_Missions = { 854, 738, 1432, 1530, 1567, 1604 };
};
