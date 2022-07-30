#pragma once
#include "CppScripts.h"

/**
 * Information about pets regarding which effect to play when a skill is cast
 */
struct PetInfo {
	const std::vector<uint32_t> effect;
	const std::string skill;
};

class DamagingPets : public CppScripts::Script {
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string message) override;
	void OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) override;
	void OnSkillEventFired(Entity* self, Entity* target, const std::string& message) override;
	void OnPlayerLoaded(Entity* self, Entity* player) override;
private:
	static void MakeUntamable(Entity* self);
	static PetInfo GetPetInfo(Entity* self);
	static void ClearEffects(Entity* self);
	static const std::map<LOT, PetInfo> petInfo;
};
