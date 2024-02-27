#ifndef __SYNCSKILL__H__
#define __SYNCSKILL__H__

#include <cstdint>
#include <string>

#include "BitStream.h"
#include "eGameMessageType.h"

/*  Message to synchronize a skill cast */
class SyncSkill {
public:
	SyncSkill() {
		bDone = false;
	}

	SyncSkill(std::string _sBitStream, uint32_t _uiBehaviorHandle, uint32_t _uiSkillHandle, bool _bDone = false) {
		bDone = _bDone;
		sBitStream = _sBitStream;
		uiBehaviorHandle = _uiBehaviorHandle;
		uiSkillHandle = _uiSkillHandle;
	}

	SyncSkill(RakNet::BitStream& stream) : SyncSkill() {
		Deserialize(stream);
	}

	~SyncSkill() {
	}

	void Serialize(RakNet::BitStream& stream) {
		stream.Write(eGameMessageType::SYNC_SKILL);

		stream.Write(bDone);
		uint32_t sBitStreamLength = sBitStream.length();
		stream.Write(sBitStreamLength);
		for (unsigned int k = 0; k < sBitStreamLength; k++) {
			stream.Write(sBitStream[k]);
		}

		stream.Write(uiBehaviorHandle);
		stream.Write(uiSkillHandle);
	}

	bool Deserialize(RakNet::BitStream& stream) {
		stream.Read(bDone);
		uint32_t sBitStreamLength{};
		stream.Read(sBitStreamLength);
		for (uint32_t k = 0; k < sBitStreamLength; k++) {
			unsigned char character;
			stream.Read(character);
			sBitStream.push_back(character);
		}

		stream.Read(uiBehaviorHandle);
		stream.Read(uiSkillHandle);

		return true;
	}

	bool bDone{};
	std::string sBitStream{};
	uint32_t uiBehaviorHandle{};
	uint32_t uiSkillHandle{};
};

#endif  //!__SYNCSKILL__H__
