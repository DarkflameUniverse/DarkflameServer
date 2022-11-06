#pragma once
#include "BaseWavesGenericEnemy.h"

class WaveBossHammerling : public BaseWavesGenericEnemy {
	void OnStartup(Entity* self) override;
	uint32_t GetPoints() override { return 1000; }
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};
