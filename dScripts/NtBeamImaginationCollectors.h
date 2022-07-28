#pragma once
#include "CppScripts.h"

class NtBeamImaginationCollectors : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	int32_t GetRandomNum();
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	int32_t m_LastRandom = 0;
	int32_t m_RandMin = 5;
	int32_t m_RandMax = 15;
	std::u16string m_FxName = u"beam_collect";
};
