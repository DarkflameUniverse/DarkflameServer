#pragma once
#include "CppScripts.h"

struct QuickBuildSet {
	std::string name;
	std::vector<std::string> effects;
};

class NsConcertQuickBuild : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnRebuildComplete(Entity* self, Entity* target) override;
	void OnDie(Entity* self, Entity* killer) override;
private:
	static std::vector<LWOOBJID> finishedQuickBuilds;
	static const float resetBlinkTime;
	static const float resetStageTime;
	static const float resetActivatorTime;
	static const float resetTime;
	static const std::map<std::string, std::string> quickBuildFX;
	static const std::map<LOT, QuickBuildSet> quickBuildSets;
	static float GetBlinkTime(float time);
	static void ProgressStageCraft(Entity* self, Entity* player);
	static void ProgressLicensedTechnician(Entity* self);
	static void UpdateEffects(Entity* self);
	static void CancelEffects(Entity* self);
};
