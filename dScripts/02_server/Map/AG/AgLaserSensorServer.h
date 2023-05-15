#pragma once
#include "CppScripts.h"

class SkillComponent;

class AgLaserSensorServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnCollisionPhantom(Entity* self, Entity* target);
private:
	float m_RepelForce = -25.0f;
	int m_SkillCastID = 163;
};

