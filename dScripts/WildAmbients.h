#pragma once
#include "CppScripts.h"

class WildAmbients : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
};
