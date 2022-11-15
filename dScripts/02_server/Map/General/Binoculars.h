#pragma once
#include "CppScripts.h"

class Binoculars : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user);
};
