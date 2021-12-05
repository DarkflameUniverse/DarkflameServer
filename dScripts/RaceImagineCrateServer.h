#pragma once
#include "CppScripts.h"

class RaceImagineCrateServer : public CppScripts::Script
{
public:
    void OnDie(Entity* self, Entity* killer) override;
};
