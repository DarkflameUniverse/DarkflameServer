#pragma once
#include "CppScripts.h"

class VanityObject;
class DLUVanityTeleportingObject : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	VanityObject* m_Object;
	float m_TeleportInterval = 15.0f;
};
