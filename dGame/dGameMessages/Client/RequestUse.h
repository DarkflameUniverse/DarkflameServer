#pragma once

#include "../GameMessage.h"

class RequestUse : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_USE; }

	bool bIsMultiInteractUse = false;
	unsigned int multiInteractID;
	int multiInteractType;
	bool secondary;
	LWOOBJID objectID;

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(bIsMultiInteractUse);
		inStream->Read(multiInteractID);
		inStream->Read(multiInteractType);
		inStream->Read(objectID);
		inStream->Read(secondary);
	};
};