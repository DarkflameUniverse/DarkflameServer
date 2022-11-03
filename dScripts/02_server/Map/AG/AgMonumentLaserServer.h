#pragma once
#include "CppScripts.h"

class AgMonumentLaserServer : public CppScripts::Script {
public:
	void OnStartup(Entity* self);
	void OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status);
private:
	float m_Radius = 25.0f;
};
