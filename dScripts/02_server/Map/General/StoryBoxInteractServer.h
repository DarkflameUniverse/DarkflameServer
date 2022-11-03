#pragma once
#include "CppScripts.h"

class StoryBoxInteractServer : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user);
};
