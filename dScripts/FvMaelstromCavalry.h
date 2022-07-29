#pragma once
#include "CppScripts.h"

class FvMaelstromCavalry : public CppScripts::Script
{
public:
	void OnStartup(Entity* self);
	void OnDie(Entity* self, Entity* killer);
};
