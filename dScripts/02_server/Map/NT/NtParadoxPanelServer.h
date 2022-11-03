#pragma once
#include "CppScripts.h"

class NtParadoxPanelServer : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
private:
	std::u16string shockAnim = u"knockback-recovery";
	float fxTime = 2.0;
	std::vector<int32_t> tPlayerOnMissions = { 1278, 1279, 1280, 1281 };
};

