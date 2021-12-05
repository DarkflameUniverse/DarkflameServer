#pragma once
#include "CppScripts.h"

class ClRing : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
