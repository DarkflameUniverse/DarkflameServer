#pragma once
#include "CppScripts.h"

class NsGetFactionMissionServer : public CppScripts::Script
{
public:
	void OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) override;
};
