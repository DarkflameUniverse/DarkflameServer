#pragma once

#include "../GameMessage.h"

class RequestLinkedMission : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_LINKED_MISSION; }

	LWOOBJID playerId{};
	int missionId{};
	bool bMissionOffered{};

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(playerId);
		inStream->Read(missionId);
		inStream->Read(bMissionOffered);
	};
};