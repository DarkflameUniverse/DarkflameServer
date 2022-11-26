#pragma once
#include "CppScripts.h"

class RainOfArrows : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	LOT m_ArrowFXObject = 15850;
	int32_t m_ArrowSkill = 1482;
	int32_t m_ArrowBehavior = 36680;
	int32_t m_ArrowDelay = 6;
	std::string m_ArrowsGUID = "{532cba3c-54da-446c-986b-128af9647bdb}";
};
