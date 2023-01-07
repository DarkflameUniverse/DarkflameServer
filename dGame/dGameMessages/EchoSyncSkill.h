#ifndef __ECHOSYNCSKILL__H__
#define __ECHOSYNCSKILL__H__

#include <string>

#include "BitStream.h"

#include "dMessageIdentifiers.h"

/*  Message to synchronize a skill cast */
class EchoSyncSkill {
	static const GAME_MSG MsgID = GAME_MSG_ECHO_SYNC_SKILL;

public:
	EchoSyncSkill() {
		bDone = false;
	}

	EchoSyncSkill(std::string _sBitStream, uint32_t _uiBehaviorHandle, uint32_t _uiSkillHandle, bool _bDone = false) {
		bDone = _bDone;
		sBitStream = _sBitStream;
		uiBehaviorHandle = _uiBehaviorHandle;
		uiSkillHandle = _uiSkillHandle;
	}

	EchoSyncSkill(RakNet::BitStream* stream) : EchoSyncSkill() {
		Deserialize(stream);
	}

	~EchoSyncSkill() {
	}

	void Serialize(RakNet::BitStream* stream) {
		stream->Write(MsgID);

		stream->Write(bDone);
		uint32_t sBitStreamLength = sBitStream.length();
		stream->Write(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			stream->Write(sBitStream[k]);
		}

		stream->Write(uiBehaviorHandle);
		stream->Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream* stream) {
		stream->Read(bDone);

		uint32_t sBitStreamLength{};
		stream->Read(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream->Read(character);
			sBitStream.push_back(character);
		}

		stream->Read(uiBehaviorHandle);
		stream->Read(uiSkillHandle);

		return true;
	}

	bool bDone{};
	std::string sBitStream{};
	uint32_t uiBehaviorHandle{};
	uint32_t uiSkillHandle{};
};

#endif  //!__ECHOSYNCSKILL__H__
