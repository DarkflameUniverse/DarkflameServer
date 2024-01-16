#pragma once
#include "CppScripts.h"

class NTImagimeterVisibility : public CppScripts::Script {
public:
	void OnQuickBuildComplete(Entity* self, Entity* target) override;
};
