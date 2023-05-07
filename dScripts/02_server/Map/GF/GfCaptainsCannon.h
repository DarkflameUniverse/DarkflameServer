#pragma once
#include "CppScripts.h"

class GfCaptainsCannon : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
private:
	int32_t m_SharkItemID = 7343;
};
