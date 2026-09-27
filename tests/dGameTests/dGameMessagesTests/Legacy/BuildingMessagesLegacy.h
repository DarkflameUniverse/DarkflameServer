#ifndef BUILDINGMESSAGESLEGACY_H
#define BUILDINGMESSAGESLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written GameMessages functions that BuildingMessages.h and the ClientPackets blueprint
// packets replaced (dGame/dGameMessages/GameMessages.cpp and dWorldServer/WorldServer.cpp, branched from origin/main
// 129199e4). Only the namespace changed. The Write* functions are the BLUEPRINT_SAVE_RESPONSE write sequences that
// were inline in HandleUnUseModel, HandleBBBSaveRequest and WorldServer's level load, wrapped in a function; the
// Read* functions are the read sequences of the replaced GameMessages::Handle* functions.

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "eBlueprintSaveResponseType.h"
#include "Entity.h"
#include "Game.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "NiPoint3.h"
#include "ServiceType.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace LegacyGameMessages {
	inline void SendStartArrangingWithItem(
		Entity* entity,
		const SystemAddress& sysAddr,
		bool bFirstTime,
		const LWOOBJID& buildAreaID,
		NiPoint3 buildStartPOS,
		int sourceBAG,
		const LWOOBJID& sourceID,
		LOT sourceLOT,
		int sourceTYPE,
		const LWOOBJID& targetID,
		LOT targetLOT,
		NiPoint3 targetPOS,
		int targetTYPE
	) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::START_ARRANGING_WITH_ITEM);

		bitStream.Write(bFirstTime);
		bitStream.Write(buildAreaID != LWOOBJID_EMPTY);
		if (buildAreaID != LWOOBJID_EMPTY) bitStream.Write(buildAreaID);
		bitStream.Write(buildStartPOS);
		bitStream.Write(sourceBAG);
		bitStream.Write(sourceID);
		bitStream.Write(sourceLOT);
		bitStream.Write(sourceTYPE);
		bitStream.Write(targetID);
		bitStream.Write(targetLOT);
		bitStream.Write(targetPOS);
		bitStream.Write(targetTYPE);

		SEND_PACKET;
	}

	inline void SendFinishArrangingWithItem(Entity* entity, const LWOOBJID& buildArea) {
		CBITSTREAM;
		CMSGHEADER;

		bool bFirstTime = true;
		const LWOOBJID& buildAreaID = buildArea;
		int newSourceBAG = 0;
		const LWOOBJID& newSourceID = LWOOBJID_EMPTY;
		LOT newSourceLOT = LOT_NULL;
		int newSourceTYPE = 0;
		const LWOOBJID& newTargetID = LWOOBJID_EMPTY;
		LOT newTargetLOT = LOT_NULL;
		int newTargetTYPE = 0;
		NiPoint3 newTargetPOS = NiPoint3();
		int oldItemBAG = 0;
		const LWOOBJID& oldItemID = LWOOBJID_EMPTY;
		LOT oldItemLOT = LOT_NULL;
		int oldItemTYPE = 0;


		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::FINISH_ARRANGING_WITH_ITEM);

		bitStream.Write(buildAreaID != LWOOBJID_EMPTY);
		if (buildAreaID != LWOOBJID_EMPTY) bitStream.Write(buildAreaID);
		bitStream.Write(newSourceBAG);
		bitStream.Write(newSourceID);
		bitStream.Write(newSourceLOT);
		bitStream.Write(newSourceTYPE);
		bitStream.Write(newTargetID);
		bitStream.Write(newTargetLOT);
		bitStream.Write(newTargetTYPE);
		bitStream.Write(newTargetPOS);
		bitStream.Write(oldItemBAG);
		bitStream.Write(oldItemID);
		bitStream.Write(oldItemLOT);
		bitStream.Write(oldItemTYPE);

		SystemAddress sysAddr = entity->GetSystemAddress();
		SEND_PACKET;
	}

	inline void SendModularBuildEnd(Entity* entity) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(entity->GetObjectID());
		bitStream.Write(MessageType::Game::MODULAR_BUILD_END);

		SystemAddress sysAddr = entity->GetSystemAddress();
		SEND_PACKET;
	}

	inline void SendSetBuildModeConfirmed(LWOOBJID objectId, const SystemAddress& sysAddr, bool start, bool warnVisitors, bool modePaused, int32_t modeValue, LWOOBJID playerId, NiPoint3 startPos) {
		CBITSTREAM;
		CMSGHEADER;

		bitStream.Write(objectId);
		bitStream.Write(MessageType::Game::SET_BUILD_MODE_CONFIRMED);

		bitStream.Write(start);
		bitStream.Write(warnVisitors);
		bitStream.Write(modePaused);
		bitStream.Write1();
		bitStream.Write(modeValue);
		bitStream.Write(playerId);
		bitStream.Write1();
		bitStream.Write(startPos);

		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) SEND_PACKET_BROADCAST;
		SEND_PACKET;
	}

	inline void SendBlueprintLoadItemResponse(const SystemAddress& sysAddr, bool success, LWOOBJID oldItemId, LWOOBJID newItemId) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::BLUEPRINT_LOAD_RESPONSE_ITEMID);
		bitStream.Write<uint8_t>(success);
		bitStream.Write<LWOOBJID>(oldItemId);
		bitStream.Write<LWOOBJID>(newItemId);
		SEND_PACKET;
	}

	// GameMessages::HandleUnUseModel, when unknown was set
	inline void WriteUnUseModelSaveResponse(const SystemAddress& sysAddr) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::BLUEPRINT_SAVE_RESPONSE);
		bitStream.Write<LWOOBJID>(LWOOBJID_EMPTY); //always zero so that a check on the client passes
		bitStream.Write(eBlueprintSaveResponseType::PlacementFailed); // Sending a non-zero error code here prevents the client from deleting its in progress build for some reason?
		bitStream.Write<uint32_t>(0);
		SEND_PACKET;
	}

	// GameMessages::HandleBBBSaveRequest's response (models: blueprint ID and the sd0 chunks written one after another)
	inline void WriteBBBSaveResponse(const SystemAddress& sysAddr, LWOOBJID localId, const std::vector<std::pair<LWOOBJID, std::vector<std::string>>>& splitLxfmls) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::BLUEPRINT_SAVE_RESPONSE);
		bitStream.Write(localId);
		bitStream.Write(eBlueprintSaveResponseType::EverythingWorked);
		bitStream.Write<uint32_t>(splitLxfmls.size());
		for (size_t i = 0; i < splitLxfmls.size(); ++i) {
			const auto blueprintID = splitLxfmls[i].first;
			// Write the ID and data to the response packet
			bitStream.Write(blueprintID);

			const auto& newSd0 = splitLxfmls[i].second;
			uint32_t newSd0Size{};
			for (const auto& chunk : newSd0) newSd0Size += chunk.size();
			bitStream.Write(newSd0Size);
			for (const auto& chunk : newSd0) bitStream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(chunk.data()), chunk.size());
		}

		SEND_PACKET;
	}

	// WorldServer's level load: the property's brick built models (blueprint ID and LXFML)
	inline void WriteLevelLoadSaveResponse(const SystemAddress& sysAddr, const std::vector<std::pair<LWOOBJID, std::string>>& bbbModels) {
		CBITSTREAM;
		BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::BLUEPRINT_SAVE_RESPONSE);
		bitStream.Write<LWOOBJID>(LWOOBJID_EMPTY); //always zero so that a check on the client passes
		bitStream.Write(eBlueprintSaveResponseType::EverythingWorked);
		bitStream.Write<uint32_t>(bbbModels.size());
		for (auto& bbbModel : bbbModels) {
			size_t lxfmlSize = bbbModel.second.size();

			// write data
			LWOOBJID blueprintID = bbbModel.first;
			bitStream.Write(blueprintID);
			bitStream.Write<uint32_t>(lxfmlSize);
			bitStream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(bbbModel.second.c_str()), lxfmlSize);
		}
		SEND_PACKET;
	}

	// GameMessages::HandleStartBuildingWithItem
	struct LegacyStartBuildingWithItem { bool firstTime{}; bool success{}; int32_t sourceBag{}; LWOOBJID sourceId{}; LOT sourceLot{}; int32_t sourceType{}; LWOOBJID targetId{}; LOT targetLot{}; NiPoint3 targetPosition{}; int32_t targetType{}; };
	inline LegacyStartBuildingWithItem ReadStartBuildingWithItem(RakNet::BitStream& inStream) {
		bool firstTime{};
		bool success{};
		int32_t sourceBag{};
		LWOOBJID sourceId{};
		LOT sourceLot{};
		int32_t sourceType{};
		LWOOBJID targetId{};
		LOT targetLot{};
		NiPoint3 targetPosition{};
		int32_t targetType{};

		inStream.Read(firstTime);
		inStream.Read(success);
		inStream.Read(sourceBag);
		inStream.Read(sourceId);
		inStream.Read(sourceLot);
		inStream.Read(sourceType);
		inStream.Read(targetId);
		inStream.Read(targetLot);
		inStream.Read(targetPosition);
		inStream.Read(targetType);
		return { firstTime, success, sourceBag, sourceId, sourceLot, sourceType, targetId, targetLot, targetPosition, targetType };
	}

	// GameMessages::HandleSetBuildMode
	struct LegacySetBuildMode { bool start{}; int32_t distanceType{}; bool modePaused{}; int modeValue{}; LWOOBJID playerId{}; NiPoint3 startPosition; };
	inline LegacySetBuildMode ReadSetBuildMode(RakNet::BitStream& inStream) {
		bool start{};
		int32_t distanceType = -1;
		bool modePaused{};
		int modeValue = 1;
		LWOOBJID playerId{};
		NiPoint3 startPosition = NiPoint3Constant::ZERO;

		inStream.Read(start);

		if (inStream.ReadBit())
			inStream.Read(distanceType);

		inStream.Read(modePaused);

		if (inStream.ReadBit())
			inStream.Read(modeValue);

		inStream.Read(playerId);

		if (inStream.ReadBit())
			inStream.Read(startPosition);
		return { start, distanceType, modePaused, modeValue, playerId, startPosition };
	}

	// GameMessages::HandleBuildModeSet (reads only the first field)
	inline bool ReadBuildModeSet(RakNet::BitStream& inStream) {
		bool bStart = false;

		inStream.Read(bStart);
		return bStart;
	}

	// GameMessages::HandleDoneArrangingWithItem
	struct LegacyDoneArranging { int newSourceBAG{}; LWOOBJID newSourceID{}; LOT newSourceLOT{}; int newSourceTYPE{}; LWOOBJID newTargetID{}; LOT newTargetLOT{}; int newTargetTYPE{}; NiPoint3 newTargetPOS; int oldItemBAG{}; LWOOBJID oldItemID{}; LOT oldItemLOT{}; int oldItemTYPE{}; };
	inline LegacyDoneArranging ReadDoneArrangingWithItem(RakNet::BitStream& inStream) {
		int newSourceBAG = 0;
		LWOOBJID newSourceID = 0;
		LOT newSourceLOT = 0;
		int newSourceTYPE = 0;
		LWOOBJID newTargetID = 0;
		LOT newTargetLOT = 0;
		int newTargetTYPE = 0;
		NiPoint3 newTargetPOS;
		int oldItemBAG = 0;
		LWOOBJID oldItemID = 0;
		LOT oldItemLOT = 0;
		int oldItemTYPE = 0;

		inStream.Read(newSourceBAG);
		inStream.Read(newSourceID);
		inStream.Read(newSourceLOT);
		inStream.Read(newSourceTYPE);
		inStream.Read(newTargetID);
		inStream.Read(newTargetLOT);
		inStream.Read(newTargetTYPE);
		inStream.Read(newTargetPOS);
		inStream.Read(oldItemBAG);
		inStream.Read(oldItemID);
		inStream.Read(oldItemLOT);
		inStream.Read(oldItemTYPE);
		return { newSourceBAG, newSourceID, newSourceLOT, newSourceTYPE, newTargetID, newTargetLOT, newTargetTYPE, newTargetPOS, oldItemBAG, oldItemID, oldItemLOT, oldItemTYPE };
	}

	// GameMessages::HandleModularBuildFinish (the parts are only read for 3 to 7 parts)
	inline std::vector<uint32_t> ReadModularBuildFinish(RakNet::BitStream& inStream) {
		uint8_t count; // 3 for rockets, 7 for cars

		inStream.Read(count);

		std::vector<uint32_t> modList;
		if (count >= 3 && count < 8) {
			for (uint32_t k = 0; k < count; k++) {
				uint32_t mod;
				inStream.Read(mod);
				modList.push_back(mod);
			}
		}
		return modList;
	}

	// GameMessages::HandleModularBuildMoveAndEquip
	inline LOT ReadModularBuildMoveAndEquip(RakNet::BitStream& inStream) {
		LOT templateID;

		inStream.Read(templateID);
		return templateID;
	}

	// GameMessages::HandleModularBuildConvertModel and HandleBBBLoadItemRequest
	inline LWOOBJID ReadObjectId(RakNet::BitStream& inStream) {
		LWOOBJID modelID;

		inStream.Read(modelID);
		return modelID;
	}

	// GameMessages::HandleUnUseModel (reads only the first two fields)
	inline std::pair<bool, LWOOBJID> ReadUnUseModel(RakNet::BitStream& inStream) {
		bool unknown{};
		LWOOBJID objIdToAddToInventory{};
		inStream.Read(unknown);
		inStream.Read(objIdToAddToInventory);
		return { unknown, objIdToAddToInventory };
	}

	// GameMessages::HandleBBBSaveRequest
	struct LegacyBBBSaveRequest { LWOOBJID localId{}; std::string sd0; uint32_t timeTaken{}; };
	inline LegacyBBBSaveRequest ReadBBBSaveRequest(RakNet::BitStream& inStream) {
		LWOOBJID localId;

		inStream.Read(localId);

		uint32_t sd0Size;
		inStream.Read(sd0Size);

		std::unique_ptr<char[]> sd0Data;
		sd0Data.reset(new char[sd0Size]);

		inStream.ReadAlignedBytes(reinterpret_cast<unsigned char*>(sd0Data.get()), sd0Size);

		uint32_t timeTaken;
		inStream.Read(timeTaken);
		return { localId, std::string(sd0Data.get(), sd0Size), timeTaken };
	}
}

#endif // BUILDINGMESSAGESLEGACY_H
