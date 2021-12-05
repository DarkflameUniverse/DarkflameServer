#pragma once
#include "CppScripts.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "dCommonVars.h"

class CatapultBouncerServer : public CppScripts::Script {
public:
    void OnRebuildComplete(Entity* self, Entity* target) override;
};
