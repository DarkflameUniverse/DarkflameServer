#pragma once
#include "CppScripts.h"

// Plays the launch effect when a player talks to the jetpack NPC.
class NpcNpJetpackGuy : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
};
