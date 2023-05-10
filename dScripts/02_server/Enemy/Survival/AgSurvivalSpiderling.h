#pragma once
#include "BaseWavesGenericEnemy.h"

class AgSurvivalSpiderling : public BaseWavesGenericEnemy {
	void OnStartup(Entity* self) override;
	uint32_t GetPoints() override;
};
