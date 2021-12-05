#pragma once
#include "CppScripts.h"

class ActPlayerDeathTrigger : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target);
};

