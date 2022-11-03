#pragma once
#include "CppScripts.h"

class NTImagimeterVisibility : public CppScripts::Script {
public:
	void OnRebuildComplete(Entity* self, Entity* target) override;
};
