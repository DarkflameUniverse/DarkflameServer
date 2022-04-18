#pragma once
#include "CppScripts.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "PhantomPhysicsComponent.h"

class WhFans : public CppScripts::Script
{
public:
	void OnStartup(Entity* self);
	void OnDie(Entity* self, Entity* killer);
	void OnFireEventServerSide(
		Entity *self,
		Entity *sender,
		std::string args,
		int32_t param1,
		int32_t param2,
		int32_t param3
	);
private:
	void ToggleFX(Entity* self, bool hit);
};

