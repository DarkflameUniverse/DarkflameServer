#pragma once
#include "CppScripts.h"
#include "BaseEnemyMech.h"

class AmDarklingMech : public BaseEnemyMech
{
public:
	void OnStartup(Entity* self) override;
};
