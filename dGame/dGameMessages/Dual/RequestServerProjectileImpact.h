#pragma once

#include "../GameMessage.h"

class RequestServerProjectileImpact : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_REQUEST_SERVER_PROJECTILE_IMPACT; }

	LWOOBJID i64LocalID;
	LWOOBJID i64TargetID;
	std::string sBitStream;

	void Serialize(RakNet::BitStream* outStream) override {
		outStream->Write(i64LocalID != LWOOBJID_EMPTY);
		if (i64LocalID != LWOOBJID_EMPTY) outStream->Write(i64LocalID);

		outStream->Write(i64TargetID != LWOOBJID_EMPTY);
		if (i64TargetID != LWOOBJID_EMPTY) outStream->Write(i64TargetID);

		uint32_t sBitStreamLength = sBitStream.length();
		outStream->Write(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			outStream->Write(sBitStream[k]);
		}
	}

	void Deserialize(RakNet::BitStream* inStream) override {
		bool i64LocalIDIsDefault{};
		inStream->Read(i64LocalIDIsDefault);
		if (i64LocalIDIsDefault != 0) inStream->Read(i64LocalID);

		bool i64TargetIDIsDefault{};
		inStream->Read(i64TargetIDIsDefault);
		if (i64TargetIDIsDefault != 0) inStream->Read(i64TargetID);

		uint32_t sBitStreamLength{};
		inStream->Read(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			inStream->Read(character);
			sBitStream.push_back(character);
		}
	}
};