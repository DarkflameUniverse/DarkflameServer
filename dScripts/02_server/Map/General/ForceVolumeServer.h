#pragma once
#include "CppScripts.h"

class ForceVolumeServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
};
