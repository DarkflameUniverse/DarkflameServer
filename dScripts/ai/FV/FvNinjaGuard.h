#pragma once
#include "CppScripts.h"

class FvNinjaGuard final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override;

private:
	LWOOBJID m_LeftGuard;
	LWOOBJID m_RightGuard;
};
