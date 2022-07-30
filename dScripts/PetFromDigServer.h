#pragma once
#include "CppScripts.h"

class PetFromDigServer : public CppScripts::Script
{
public:
	void OnStartup(Entity* self) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
	void OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, eNotifyType type) override;
};
