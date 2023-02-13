#pragma once
#include "CppScripts.h"

class BaseEnemyApe : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnSkillCast(Entity* self, uint32_t skillID) override;
	void OnHit(Entity* self, Entity* attacker) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
private:
	static void StunApe(Entity* self, bool stunState);
};
