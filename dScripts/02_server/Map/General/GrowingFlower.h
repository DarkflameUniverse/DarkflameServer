#pragma once
#include "CppScripts.h"

class GrowingFlower : public CppScripts::Script {
public:
	void OnSkillEventFired(Entity* self, Entity* target, const std::string& message) override;
	void OnTimerDone(Entity* self, std::string message) override;
private:
	static const std::vector<uint32_t> achievementIDs;
	constexpr static const float aliveTime = 16.0f;
};
