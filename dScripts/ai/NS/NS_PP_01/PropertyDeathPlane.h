#pragma once
#include "CppScripts.h"

class PropertyDeathPlane : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};

