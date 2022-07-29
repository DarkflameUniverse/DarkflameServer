#pragma once
#include "CppScripts.h"

class AmSkullkinDrill : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;

	Entity* GetStandObj(Entity* self);

	void OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) override;

	void TriggerDrill(Entity* self);

	void OnWaypointReached(Entity* self, uint32_t waypointIndex) override;

	void OnUse(Entity* self, Entity* user) override;

	void FreezePlayer(Entity* self, Entity* player, bool bFreeze);

	void OnArrived(Entity* self, uint32_t waypointIndex);

	void PlayCinematic(Entity* self);

	void PlayAnim(Entity* self, Entity* player, const std::string& animName);

	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;

	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	std::vector<int32_t> m_MissionsToUpdate = { 972, 1305, 1308 };
};
