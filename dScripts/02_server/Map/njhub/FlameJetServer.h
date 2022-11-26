#pragma once
#include "CppScripts.h"

class FlameJetServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnCollisionPhantom(Entity* self, Entity* target) override;
	void OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) override;
};
