#pragma once
#include "CppScripts.h"

class FvConsoleLeftQuickbuild : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) override;
	void OnUse(Entity* self, Entity* user) override;
};
