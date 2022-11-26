#pragma once
#include "CppScripts.h"
#include <map>

static std::map<std::u16string, uint32_t> ElementFlags = {
	{u"earth", ePlayerFlags::NJ_EARTH_SPINJITZU},
	{u"lightning", ePlayerFlags::NJ_LIGHTNING_SPINJITZU},
	{u"ice", ePlayerFlags::NJ_ICE_SPINJITZU},
	{u"fire", ePlayerFlags::NJ_FIRE_SPINJITZU}
};

static std::map<std::u16string, uint32_t> ElementMissions = {
	{u"earth", 1796},
	{u"lightning", 1952},
	{u"ice", 1959},
	{u"fire", 1962},
};

class NjNPCMissionSpinjitzuServer : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
private:
	const std::u16string ElementVariable = u"element";
};
