#pragma once
#include "CppScripts.h"

class NtXRayServer : public CppScripts::Script
{
public:
	void OnCollisionPhantom(Entity* self, Entity* target) override;
};
