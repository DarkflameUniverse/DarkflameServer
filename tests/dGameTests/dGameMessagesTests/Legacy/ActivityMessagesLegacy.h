#ifndef ACTIVITYMESSAGESLEGACY_H
#define ACTIVITYMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages::Send* functions that ActivityMessages.h replaced
// (dGame/dGameMessages/GameMessages.cpp at origin/main 129199e4). Only the namespace changed.
// The byte-equality tests send the same inputs through these and through the new structs and require
// identical bytes, so the wire format is pinned even after the production code is deleted.

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "ServiceType.h"

#include <cmath>
#include <string>

namespace LegacyGameMessages {
	inline void SendActivityPause(LWOOBJID objectId, bool pause = false, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ACTIVITY_PAUSE);
		bitStream.Write(pause);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendStartActivityTime(LWOOBJID objectId, float_t startTime, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::START_ACTIVITY_TIME);
		bitStream.Write<float_t>(startTime);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRequestActivityEnter(LWOOBJID objectId, const SystemAddress& sysAddr, bool bStart, LWOOBJID userID) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::REQUEST_ACTIVITY_ENTER);
		bitStream.Write<bool>(bStart);
		bitStream.Write<LWOOBJID>(userID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendActivityEnter(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ACTIVITY_ENTER);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendActivityStart(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ACTIVITY_START);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendActivityExit(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ACTIVITY_EXIT);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendActivityStop(LWOOBJID objectId, bool bExit, bool bUserCancel, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ACTIVITY_STOP);

		bitStream.Write(bExit);
		bitStream.Write(bUserCancel);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendShowActivityCountdown(LWOOBJID objectId, bool bPlayAdditionalSound, bool bPlayCountdownSound, std::u16string sndName, int32_t stateToPlaySoundOn, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SHOW_ACTIVITY_COUNTDOWN);

		bitStream.Write(bPlayAdditionalSound);

		bitStream.Write(bPlayCountdownSound);

		bitStream.Write<uint32_t>(sndName.size());
		for (auto character : sndName) {
			bitStream.Write(character);
		}

		bitStream.Write(stateToPlaySoundOn);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	// Inbound: the read sequence of the old GameMessages::HandleRequestActivityExit, returning what it read.
	struct LegacyRequestActivityExitRead {
		bool canceled = false;
		LWOOBJID player_id = LWOOBJID_EMPTY;
	};
	inline LegacyRequestActivityExitRead ReadRequestActivityExit(RakNet::BitStream& inStream) {
		LegacyRequestActivityExitRead result;
		inStream.Read(result.canceled);
		if (!result.canceled) return result;

		inStream.Read(result.player_id);
		return result;
	}
}

#endif // ACTIVITYMESSAGESLEGACY_H
