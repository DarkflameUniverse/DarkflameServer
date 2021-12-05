#pragma once
#include "CppScripts.h"

class AmDropshipComputer : public CppScripts::Script
{
public:
    void OnStartup(Entity* self) override;
    void OnUse(Entity* self, Entity* user) override;
    void OnDie(Entity* self, Entity* killer) override;
    void OnTimerDone(Entity* self, std::string timerName) override;
};
