#pragma once
#include "BaseWavesGenericEnemy.h"

class WaveBossSpiderling : public BaseWavesGenericEnemy {
	uint32_t GetPoints() override { return 5000; }
	void OnStartup(Entity* self) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
		int32_t param3) override;
};
