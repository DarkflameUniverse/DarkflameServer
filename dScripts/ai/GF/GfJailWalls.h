#pragma once
#include "CppScripts.h"

class GfJailWalls final : public CppScripts::Script
{
public:
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
};
