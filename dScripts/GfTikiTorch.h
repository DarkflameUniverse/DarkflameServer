#pragma once
#include "CppScripts.h"

class GfTikiTorch : public CppScripts::Script 
{
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* killer) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
    void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;
private:
	void LightTorch(Entity* self);

	LOT m_imaginationlot = 935;
	int32_t m_numspawn = 3;
};