#pragma once
#include "CppScripts.h"
#include <map>
#include "ePlayerFlag.h"

static std::map<std::u16string, ePlayerFlag> ElementFlags = {
	{u"earth", ePlayerFlag::NJ_EARTH_SPINJITZU},
	{u"lightning", ePlayerFlag::NJ_LIGHTNING_SPINJITZU},
	{u"ice", ePlayerFlag::NJ_ICE_SPINJITZU},
	{u"fire", ePlayerFlag::NJ_FIRE_SPINJITZU}
};

static std::map<std::u16string, uint32_t> ElementMissions = {
	{u"earth", 1796},
	{u"lightning", 1952},
	{u"ice", 1959},
	{u"fire", 1962},
};

class NjNPCMissionSpinjitzuServer : public CppScripts::Script {
public:
	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) override;
private:
	const std::u16string ElementVariable = u"element";
};
