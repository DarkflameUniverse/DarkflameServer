#pragma once
#include "CppScripts.h"

class Lieutenant : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	void OnDie(Entity* self, Entity* killer) override;
};

