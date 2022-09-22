#pragma once
#include "CppScripts.h"

class NpcNjAssistantServer : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState);

private:
	int mailMission = 1728; //mission to get the item out of your mailbox
	int mailAchievement = 1729; // fun fact: spelled "Achivement" in the actual script
};
