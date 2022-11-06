#pragma once
#include "CppScripts.h"

class AgSalutingNpcs final : public CppScripts::Script
{
public:
	void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override;
};
