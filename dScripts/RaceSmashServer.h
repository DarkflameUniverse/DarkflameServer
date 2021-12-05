#pragma once
#include "CppScripts.h"

class RaceSmashServer : public CppScripts::Script {
    void OnDie(Entity *self, Entity *killer) override;
};
