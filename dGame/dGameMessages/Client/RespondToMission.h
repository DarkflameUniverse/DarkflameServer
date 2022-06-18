#pragma once

#include "../GameMessage.h"

class RespondToMission : public GameMessage{
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_RESPOND_TO_MISSION; }

	int missionID{};
	LWOOBJID playerID{};
	LWOOBJID receiverID{};
	bool isDefaultReward{};
	LOT reward = LOT_NULL;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(missionID);
		inStream->Read(playerID);
		inStream->Read(receiverID);
		inStream->Read(isDefaultReward);
		if (isDefaultReward) inStream->Read(reward);
	};
};