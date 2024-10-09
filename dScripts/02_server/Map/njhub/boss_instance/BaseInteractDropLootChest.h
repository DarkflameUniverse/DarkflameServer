#pragma once
#include "CppScripts.h"

class BaseInteractDropLootChest : public CppScripts::Script
{
public:
	virtual void OnUse(Entity* self, Entity* user) override;
	void BaseUse(Entity* self, Entity* user);
	void PostTokenUse(Entity* self, Entity* user);

private:
	std::vector<int32_t> MissionIds = { 2209, 2210, 2211 };
};

