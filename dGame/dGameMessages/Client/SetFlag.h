#pragma once

#include "../GameMessage.h"

class SetFlag : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_SET_FLAG; }

	bool bFlag{};
	int iFlagID{};

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(bFlag);
		inStream->Read(iFlagID);
	};
};