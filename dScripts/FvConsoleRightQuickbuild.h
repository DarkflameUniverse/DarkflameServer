#pragma once
#include "CppScripts.h"

class FvConsoleRightQuickbuild : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
	void OnUse(Entity* self, Entity* user) override;
};
