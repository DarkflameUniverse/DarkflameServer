#pragma once
#include "CppScripts.h"

class NtCombatChallengeServer : public CppScripts::Script
{
public:
	void OnUse(Entity* self, Entity* user) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
	void OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) override;
	void SpawnTargetDummy(Entity* self);
	void SetAttackImmunity(LWOOBJID objID, bool bTurnOn);
	void OnChildLoaded(Entity* self, Entity* child);
	void ResetGame(Entity* self);
	void OnActivityTimerUpdate(Entity* self, float timeRemaining);
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	float gameTime = 30.0f;
	std::string startSound = "{a477f897-30da-4b15-8fce-895c6547adae}";
	std::string stopSound = "{a832b9c5-b000-4c97-820a-2a7d1e68dd9d}";
	std::string timerSound = "{79b38431-4fc7-403b-8ede-eaff700a7ab0}";
	std::string timerLowSound = "{0e1f1284-e1c4-42ed-8ef9-93e8756948f8}";
	std::string scoreSound = "{cfdade40-3d97-4cf5-b53c-862e0b84c1a1}";

	std::vector<LOT> tTargets = {
					13556, 13556, 13764, 13764, 13765, 13765,
					13766, 13766, 13767, 13767, 13768, 13768,
					13830, 13769, 13769, 13770, 13830, 13770,
					13771, 13771, 13830, 13772
	};

	struct MissionRequirements
	{
		int32_t mission;
		int32_t damage;
	};

	std::vector<MissionRequirements> tMissions = {
		{1010, 25},
		{1340, 100},
		{1341, 240},
		{1342, 290}
	};
};
