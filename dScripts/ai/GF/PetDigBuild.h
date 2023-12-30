#pragma once
#include "CppScripts.h"

class PetDigBuild : public CppScripts::Script
{
public:
	void OnQuickBuildComplete(Entity* self, Entity* target);
	void OnDie(Entity* self, Entity* killer);
};
