#pragma once
#include "CppScripts.h"

class TokenConsoleServer : public CppScripts::Script {
	void OnUse(Entity* self, Entity* user) override;

private:
	int bricksToTake = 25;
	int tokensToGive = 5;
};
