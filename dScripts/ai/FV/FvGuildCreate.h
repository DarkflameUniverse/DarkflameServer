#pragma once
#include "CppScripts.h"

// The Guild Master (LOT 3001, scripts\ai\FV\L_GUILD_CREATE.lua): using it opens the guild create box (docs/Guilds.md).
// Live never placed it in a zone.
class FvGuildCreate : public CppScripts::Script {
public:
	void OnUse(Entity* self, Entity* user) override;
};
