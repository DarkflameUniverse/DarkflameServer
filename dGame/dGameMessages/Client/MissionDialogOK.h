#pragma once

#include "../GameMessage.h"
#include "MissionState.h"

class MissionDialogOK : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_MISSION_DIALOGUE_OK; }

	bool bIsComplete{};
	MissionState iMissionState{};
	int missionID{};
	LWOOBJID responder{};

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(bIsComplete);
		inStream->Read(iMissionState);
		inStream->Read(missionID);
		inStream->Read(responder);
	}
};