#pragma once
#include "ActivityManager.h"

class MinigameTreasureChestServer : public ActivityManager {
public:
	void OnStartup(Entity* self) override;
	void OnUse(Entity* self, Entity* user) override;
	uint32_t CalculateActivityRating(Entity* self, LWOOBJID playerID) override;
private:
	const uint32_t frakjawChestId = 16486;
};
