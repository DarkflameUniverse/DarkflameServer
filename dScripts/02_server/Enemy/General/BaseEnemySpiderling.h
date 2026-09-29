#pragma once
#include "CppScripts.h"

// L_BASE_ENEMY_SPIDERLING: spiderlings can't be stunned, interrupted, knocked back or pulled
class BaseEnemySpiderling : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
};
