#pragma once
#include "CppScripts.h"
#include "PhantomPhysicsComponent.h"
#include "SkillComponent.h"
#include "EntityManager.h"
#include "AgMonumentLaserServer.h"

class AgLaserSensorServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnCollisionPhantom(Entity* self, Entity* target);
private:
	SkillComponent* m_Skill;
	int m_RepelForce = -25;
	int m_SkillCastID = 163;
};

