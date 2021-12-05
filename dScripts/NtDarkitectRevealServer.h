#pragma once
#include "CppScripts.h"

class NtDarkitectRevealServer : public CppScripts::Script
{
public:
    void OnUse(Entity* self, Entity* user) override;
    void Darkitect(Entity* self, Entity* player);
};
