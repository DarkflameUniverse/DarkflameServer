#pragma once
#include "CppScripts.h"

class RemoveRentalGear : public CppScripts::Script {
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState);

private:
	int defaultMission = 768;	                            //mission to remove gearSets on completion
	std::vector<int> gearSets = { 14359,14321,14353,14315 };	//inventory items to remove
	int equipFlag = 126; //Set upon wearing trial faction armor for the first time in a session
};

