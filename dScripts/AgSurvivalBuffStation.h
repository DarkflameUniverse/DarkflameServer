#pragma once
#include "CppScripts.h"

class AgSurvivalBuffStation : public CppScripts::Script
{
public:
    void OnStartup(Entity* self) override;
    void OnRebuildComplete(Entity* self, Entity* target) override;
};