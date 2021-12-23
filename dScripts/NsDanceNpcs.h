#pragma once
#include "CppScripts.h"

class NsDanceNpcs final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override;
};
