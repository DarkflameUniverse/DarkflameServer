#ifndef PETMESSAGESLEGACY_H
#define PETMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages functions that PetMessages.h replaced
// (dGame/dGameMessages/GameMessages.cpp, branched from origin/main 129199e4). Only the namespace changed.
// The Read* functions are the read sequences of the replaced GameMessages::Handle* functions.

#include "LegacyPacketMacros.h"
#include "BitStreamUtils.h"
#include "Brick.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "ePetAbilityType.h"
#include "ePetTamingNotifyType.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "ServiceType.h"

#include <string>
#include <vector>

namespace LegacyGameMessages {
	inline void SendNotifyPetTamingMinigame(LWOOBJID objectId, LWOOBJID petId, LWOOBJID playerTamingId, bool bForceTeleport, ePetTamingNotifyType notifyType, NiPoint3 petsDestPos, NiPoint3 telePos, NiQuaternion teleRot, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::NOTIFY_PET_TAMING_MINIGAME);

		bitStream.Write(petId);
		bitStream.Write(playerTamingId);
		bitStream.Write(bForceTeleport);
		bitStream.Write(notifyType);
		bitStream.Write(petsDestPos);
		bitStream.Write(telePos);

		const bool hasDefault = teleRot != QuatUtils::IDENTITY;
		bitStream.Write(hasDefault);
		if (hasDefault) bitStream.Write(teleRot);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendNotifyTamingModelLoadedOnServer(LWOOBJID objectId, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::NOTIFY_TAMING_MODEL_LOADED_ON_SERVER);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendNotifyPetTamingPuzzleSelected(LWOOBJID objectId, const std::vector<Brick>& bricks, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::NOTIFY_TAMING_PUZZLE_SELECTED);

		bitStream.Write<uint32_t>(bricks.size());
		for (const auto& brick : bricks) {
			bitStream.Write(brick.designerID);
			bitStream.Write(brick.materialID);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPetTamingTryBuildResult(LWOOBJID objectId, bool bSuccess, int32_t iNumCorrect, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PET_TAMING_TRY_BUILD_RESULT);

		bitStream.Write(bSuccess);
		bitStream.Write(iNumCorrect != 0);
		if (iNumCorrect != 0) bitStream.Write(iNumCorrect);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPetResponse(LWOOBJID objectId, LWOOBJID objIDPet, int32_t iPetCommandType, int32_t iResponse, int32_t iTypeID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PET_RESPONSE);

		bitStream.Write(objIDPet);
		bitStream.Write(iPetCommandType);
		bitStream.Write(iResponse);
		bitStream.Write(iTypeID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendAddPetToPlayer(LWOOBJID objectId, int32_t iElementalType, std::u16string name, LWOOBJID petDBID, LOT petLOT, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::ADD_PET_TO_PLAYER);

		bitStream.Write(iElementalType);
		bitStream.Write<uint32_t>(name.size());
		for (const auto character : name) {
			bitStream.Write(character);
		}

		bitStream.Write(petDBID);
		bitStream.Write(petLOT);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRegisterPetID(LWOOBJID objectId, LWOOBJID objID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::REGISTER_PET_ID);

		bitStream.Write(objID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendRegisterPetDBID(LWOOBJID objectId, LWOOBJID petDBID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::REGISTER_PET_DBID);

		bitStream.Write(petDBID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendClientExitTamingMinigame(LWOOBJID objectId, bool bVoluntaryExit, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::CLIENT_EXIT_TAMING_MINIGAME);

		bitStream.Write(bVoluntaryExit);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendShowPetActionButton(const LWOOBJID objectId, const ePetAbilityType petAbility, const bool bShow, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SHOW_PET_ACTION_BUTTON);

		bitStream.Write(petAbility);
		bitStream.Write(bShow);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendBouncerActiveStatus(LWOOBJID objectId, bool bActive, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::BOUNCER_ACTIVE_STATUS);

		bitStream.Write(bActive);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendSetPetName(LWOOBJID objectId, std::u16string name, LWOOBJID petDBID, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SET_PET_NAME);

		bitStream.Write<uint32_t>(name.size());
		for (const auto character : name) {
			bitStream.Write(character);
		}

		bitStream.Write(petDBID != LWOOBJID_EMPTY);
		if (petDBID != LWOOBJID_EMPTY) bitStream.Write(petDBID);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendSetPetNameModerated(LWOOBJID objectId, LWOOBJID petDBID, int32_t nModerationStatus, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SET_PET_NAME_MODERATED);

		bitStream.Write(petDBID != LWOOBJID_EMPTY);
		if (petDBID != LWOOBJID_EMPTY) bitStream.Write(petDBID);

		bitStream.Write(nModerationStatus);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendPetNameChanged(LWOOBJID objectId, int32_t moderationStatus, std::u16string name, std::u16string ownerName, const SystemAddress& sysAddr) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::PET_NAME_CHANGED);

		bitStream.Write(moderationStatus);

		bitStream.Write<uint32_t>(name.size());
		for (const auto character : name) {
			bitStream.Write(character);
		}

		bitStream.Write<uint32_t>(ownerName.size());
		for (const auto character : ownerName) {
			bitStream.Write(character);
		}

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	// GameMessages::HandleClientExitTamingMinigame
	inline bool ReadClientExitTamingMinigame(RakNet::BitStream& inStream) {
		bool bVoluntaryExit = inStream.ReadBit();
		return bVoluntaryExit;
	}

	// GameMessages::HandlePetTamingTryBuild
	struct LegacyPetTamingTryBuild { std::vector<Brick> bricks; bool clientFailed{}; bool rejected{}; };
	inline LegacyPetTamingTryBuild ReadPetTamingTryBuild(RakNet::BitStream& inStream) {
		uint32_t brickCount;
		std::vector<Brick> bricks;
		bool clientFailed;

		inStream.Read(brickCount);

		if (brickCount > MAX_MESSAGE_LENGTH) return { {}, false, true }; // Prevent DoS via unbounded brick count

		bricks.reserve(brickCount);

		for (uint32_t i = 0; i < brickCount; i++) {
			Brick brick;

			inStream.Read(brick);

			bricks.push_back(brick);
		}

		clientFailed = inStream.ReadBit();
		return { bricks, clientFailed, false };
	}

	// GameMessages::HandleNotifyTamingBuildSuccess
	inline NiPoint3 ReadNotifyTamingBuildSuccess(RakNet::BitStream& inStream) {
		NiPoint3 position;

		inStream.Read(position);
		return position;
	}

	// GameMessages::HandleRequestSetPetName
	struct LegacyRequestSetPetName { std::u16string name; bool rejected{}; };
	inline LegacyRequestSetPetName ReadRequestSetPetName(RakNet::BitStream& inStream) {
		uint32_t nameLength;
		std::u16string name;

		inStream.Read(nameLength);

		if (nameLength > MAX_MESSAGE_LENGTH) return { {}, true };
		for (size_t i = 0; i < nameLength; i++) {
			char16_t character;
			inStream.Read(character);
			name.push_back(character);
		}
		return { name, false };
	}

	// GameMessages::HandleCommandPet
	struct LegacyCommandPet { NiPoint3 genericPosInfo; LWOOBJID objIdSource{}; int32_t iPetCommandType{}; int32_t iTypeID{}; bool overrideObey{}; };
	inline LegacyCommandPet ReadCommandPet(RakNet::BitStream& inStream) {
		NiPoint3 genericPosInfo;
		LWOOBJID objIdSource;
		int32_t iPetCommandType;
		int32_t iTypeID;
		bool overrideObey;

		inStream.Read(genericPosInfo);
		inStream.Read(objIdSource);
		inStream.Read(iPetCommandType);
		inStream.Read(iTypeID);
		overrideObey = inStream.ReadBit();
		return { genericPosInfo, objIdSource, iPetCommandType, iTypeID, overrideObey };
	}

	// GameMessages::HandleDespawnPet
	inline bool ReadDespawnPet(RakNet::BitStream& inStream) {
		bool bDeletePet;

		bDeletePet = inStream.ReadBit();
		return bDeletePet;
	}
}

#endif // PETMESSAGESLEGACY_H
