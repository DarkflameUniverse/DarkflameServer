#pragma once

#include "../GameMessage.h"

class SyncSkill : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_SYNC_SKILL; }

	bool bDone{};
	std::string sBitStream{};
	unsigned int uiBehaviorHandle{};
	unsigned int uiSkillHandle{};

	void Serialize(RakNet::BitStream* outStream) {
		outStream->Write(bDone);
		uint32_t sBitStreamLength = sBitStream.length();
		outStream->Write(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			outStream->Write(sBitStream[k]);
		}

		outStream->Write(uiBehaviorHandle);
		outStream->Write(uiSkillHandle);
	}

	void Deserialize(RakNet::BitStream* inStream) {
		inStream->Read(bDone);
		uint32_t sBitStreamLength{};
		inStream->Read(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			inStream->Read(character);
			sBitStream.push_back(character);
		}
		inStream->Read(uiBehaviorHandle);
		inStream->Read(uiSkillHandle);
	}
};