#pragma once
#include "BaseEnemyMech.h"

class VeMech : public BaseEnemyMech {
public:
	void OnStartup(Entity* self) override;
};
