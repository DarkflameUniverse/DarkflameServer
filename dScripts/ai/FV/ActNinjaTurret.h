#pragma once
#include "CppScripts.h"

class ActNinjaTurret : public CppScripts::Script
{
public:
	void OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};

