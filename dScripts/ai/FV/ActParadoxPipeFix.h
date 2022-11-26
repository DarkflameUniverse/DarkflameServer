#pragma once
#include "CppScripts.h"

class ActParadoxPipeFix : public CppScripts::Script
{
public:
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnRebuildNotifyState(Entity* self, eRebuildState state) override;
};

