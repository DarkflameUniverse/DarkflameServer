#pragma once
#include "CppScripts.h"

class GfArchway : public CppScripts::Script {
public:
	void OnRebuildComplete(Entity* self, Entity* target) override;
private:
	const uint32_t SHIELDING_SKILL = 863;
	const uint32_t SHIELDING_BEHAVIOR = 3788;
};
