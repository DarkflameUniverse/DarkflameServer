#pragma once
#include "CppScripts.h"

class AgShipPlayerDeathTrigger : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target);
};

