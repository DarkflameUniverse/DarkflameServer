#pragma once
#include "CppScripts.h"
#include "ChooseYourDestinationNsToNt.h"
#include "BaseConsoleTeleportServer.h"
#include "AMFFormat.h"
#include "BaseEnemyMech.h"

class AmDarklingMech : public BaseEnemyMech
{
public:
    void OnStartup(Entity* self) override;
};
