#pragma once
#include "CppScripts.h"

class VanityNPC;
class DLUVanityNPC : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;

private:
	VanityNPC* m_NPC;
};
