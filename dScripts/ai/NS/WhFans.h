#pragma once
#include "CppScripts.h"
class WhFans : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnDie(Entity* self, Entity* killer) override;
	void OnFireEventServerSide(
		Entity* self,
		Entity* sender,
		std::string args,
		int32_t param1,
		int32_t param2,
		int32_t param3
	) override;
private:
	void ToggleFX(Entity* self, bool hit);
};

