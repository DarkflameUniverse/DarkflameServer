#include "NjNPCMissionSpinjitzuServer.h"

class NjColeNPC : public NjNPCMissionSpinjitzuServer {
	void OnEmoteReceived(Entity* self, int32_t emote, Entity* target) override;

	void OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) override;
};
