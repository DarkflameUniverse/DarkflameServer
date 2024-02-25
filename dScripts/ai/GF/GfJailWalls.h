#pragma once
#include "CppScripts.h"

class GfJailWalls final : public CppScripts::Script
{
public:
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
	void OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) override;
};
