#pragma once
#include "CppScripts.h"

class AgImagSmashable : public CppScripts::Script {
public:
	void OnDie(Entity* self, Entity* killer);
private:
	void CrateAnimal(Entity* self);
};
