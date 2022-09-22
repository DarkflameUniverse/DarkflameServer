#pragma once
#include "CppScripts.h"

class NtSleepingGuard final : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	std::vector<int32_t> m_ValidEmotes = { 175,372,354,356,374,115,210,373,392,352,364,69,174,386,375,384,376,385,393,383 };
};
