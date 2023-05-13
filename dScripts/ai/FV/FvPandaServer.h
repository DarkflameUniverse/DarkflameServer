#pragma once
#include "CppScripts.h"

class FvPandaServer : public CppScripts::Script {
	void OnStartup(Entity* self) override;
	void OnNotifyPetTamingMinigame(Entity* self, Entity* tamer, ePetTamingNotifyType type) override;
	void OnTimerDone(Entity* self, std::string timerName) override;
};
