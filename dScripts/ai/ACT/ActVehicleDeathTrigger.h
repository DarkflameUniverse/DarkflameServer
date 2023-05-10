#pragma once
#include "CppScripts.h"

class ActVehicleDeathTrigger : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};

