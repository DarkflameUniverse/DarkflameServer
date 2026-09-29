#include "ObjectMessages.h"

#include "BitStreamUtils.h"
#include "Character.h"
#include "CharacterComponent.h"
#include "ClientPackets.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "MissionComponent.h"
#include "MissionOfferComponent.h"
#include "PlayerManager.h"
#include "RocketLaunchpadControlComponent.h"
#include "ZoneMessages.h"
#include "ZoneInstanceManager.h"
#include "dServer.h"
#include "dZoneManager.h"
#include "eMissionTaskType.h"
#include "eReplicaComponentType.h"

namespace GameMessages {
	void FireEventClientSide::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, args);
		bitStream.Write(object);
		BitStreamUtils::WriteOptional<int64_t>(bitStream, param1, 0);
		BitStreamUtils::WriteOptional<int32_t>(bitStream, param2, -1);
		bitStream.Write(senderID);
	}

	bool FireEventClientSide::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, args));
		VALIDATE_READ(bitStream.Read(object));
		VALIDATE_READ(BitStreamUtils::ReadOptional<int64_t>(bitStream, param1, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional<int32_t>(bitStream, param2, -1));
		VALIDATE_READ(bitStream.Read(senderID));
		return true;
	}

	void FireEventServerSide::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, args);
		BitStreamUtils::WriteOptional(bitStream, param1, -1);
		BitStreamUtils::WriteOptional(bitStream, param2, -1);
		BitStreamUtils::WriteOptional(bitStream, param3, -1);
		bitStream.Write(senderID);
	}

	bool FireEventServerSide::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, args));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, param1, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, param2, -1));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, param3, -1));
		VALIDATE_READ(bitStream.Read(senderID));
		return true;
	}

	void FireEventServerSide::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* sender = Game::entityManager->GetEntity(senderID);
		auto* player = PlayerManager::GetPlayer(sysAddr);

		if (!player) {
			return;
		}

		// This should probably get it's own "ServerEvents" system or something at some point
		if (args == u"ZonePlayer") {
			// Should probably check to make sure they're using a launcher at some point before someone makes a hack that lets you testmap

			LWOCLONEID cloneId = 0;
			LWOMAPID mapId = 0;

			auto* rocketPad = entity.GetComponent<RocketLaunchpadControlComponent>();

			if (rocketPad == nullptr) return;

			cloneId = rocketPad->GetSelectedCloneId(player->GetObjectID());

			if (param2) {
				mapId = rocketPad->GetDefaultZone();
			} else {
				mapId = param3;
			}

			if (mapId == 0) {
				mapId = rocketPad->GetSelectedMapId(player->GetObjectID());
			}

			if (mapId == 0) {
				mapId = Game::zoneManager->GetZoneID().GetMapID(); // Fallback to sending the player back to the same zone.
			}

			LOG("Player %llu has requested zone transfer to (%i, %i).", sender->GetObjectID(), static_cast<int>(mapId), static_cast<int>(cloneId));

			auto* character = player->GetCharacter();

			if (mapId <= 0) {
				return;
			}

			// Live counted the rocket when the client asked to go (after the launch), before TransferToZone
			auto* launcher = player->GetComponent<CharacterComponent>();
			if (launcher) launcher->UpdatePlayerStatistic(RocketsUsed);

			ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, mapId, cloneId, false, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
				LOG("Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i", character->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

				if (character) {
					auto* characterComponent = player->GetComponent<CharacterComponent>();
					if (characterComponent) {
						characterComponent->AddVisitedLevel(LWOZONEID(zoneID, LWOINSTANCEID_INVALID, zoneClone));
					}

					character->SetZoneID(zoneID);
					character->SetZoneInstance(zoneInstance);
					character->SetZoneClone(zoneClone);
				}

				GameMessages::SendZoneTransferNotice(player->GetObjectID(), static_cast<LWOMAPID>(zoneID), zoneClone, character ? GeneralUtils::ASCIIToUTF16(character->GetTargetScene()) : u"", sysAddr);

				ClientPackets::TransferToWorld transfer;
				transfer.serverIP = LUString(serverIP);
				transfer.serverPort = serverPort;
				transfer.mythranShift = mythranShift;
				transfer.Send(sysAddr);
				return;
				});
		}

		entity.OnFireEventServerSide(sender, GeneralUtils::UTF16ToWTF8(args), param1, param2, param3);
	}

	namespace {
		template<typename Msg>
		void WriteNotify(RakNet::BitStream& bitStream, const Msg& msg) {
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, msg.name);
			bitStream.Write(msg.param1);
			bitStream.Write(msg.param2);
			bitStream.Write(msg.paramObj);
			BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, msg.paramStr);
		}

		template<typename Msg>
		bool ReadNotify(RakNet::BitStream& bitStream, Msg& msg) {
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, msg.name));
			VALIDATE_READ(bitStream.Read(msg.param1));
			VALIDATE_READ(bitStream.Read(msg.param2));
			VALIDATE_READ(bitStream.Read(msg.paramObj));
			VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, msg.paramStr));
			return true;
		}
	}

	void NotifyClientObject::Serialize(RakNet::BitStream& bitStream) const {
		WriteNotify(bitStream, *this);
	}

	bool NotifyClientObject::Deserialize(RakNet::BitStream& bitStream) {
		return ReadNotify(bitStream, *this);
	}

	void NotifyClientZoneObject::Serialize(RakNet::BitStream& bitStream) const {
		WriteNotify(bitStream, *this);
	}

	bool NotifyClientZoneObject::Deserialize(RakNet::BitStream& bitStream) {
		return ReadNotify(bitStream, *this);
	}

	void NotifyObject::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objIDSender);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
		bitStream.Write(param1);
		bitStream.Write(param2);
	}

	bool NotifyObject::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objIDSender));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name));
		VALIDATE_READ(bitStream.Read(param1));
		VALIDATE_READ(bitStream.Read(param2));
		return true;
	}

	void ScriptNetworkVarUpdate::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteNameValueText(bitStream, tableOfVars);
	}

	bool ScriptNetworkVarUpdate::Deserialize(RakNet::BitStream& bitStream) {
		return BitStreamUtils::ReadNameValueText(bitStream, tableOfVars);
	}

	void NotifyClientFailedPrecondition::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, failedReason);
		bitStream.Write(preconditionID);
	}

	bool NotifyClientFailedPrecondition::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, failedReason));
		VALIDATE_READ(bitStream.Read(preconditionID));
		return true;
	}

	void TerminateInteraction::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(terminator);
		bitStream.Write(type);
	}

	bool TerminateInteraction::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(terminator));
		VALIDATE_READ(bitStream.Read(type));
		return true;
	}

	void SetName::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, name);
	}

	bool SetName::Deserialize(RakNet::BitStream& bitStream) {
		return BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, name);
	}

	void RequestUse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bIsMultiInteractUse);
		bitStream.Write(multiInteractID);
		bitStream.Write(multiInteractType);
		bitStream.Write(object);
		bitStream.Write(secondary);
	}

	bool RequestUse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bIsMultiInteractUse));
		VALIDATE_READ(bitStream.Read(multiInteractID));
		VALIDATE_READ(bitStream.Read(multiInteractType));
		VALIDATE_READ(bitStream.Read(object));
		VALIDATE_READ(bitStream.Read(secondary));
		return true;
	}

	void RequestUse::Handle(Entity& entity, const SystemAddress& sysAddr) {
		Entity* interactedObject = Game::entityManager->GetEntity(object);

		if (interactedObject == nullptr) {
			LOG("Object %llu tried to interact, but doesn't exist!", object);

			return;
		}

		if (interactedObject->GetLOT() == 9524) {
			entity.GetCharacter()->SetBuildMode(true);
		}

		if (bIsMultiInteractUse) {
			if (multiInteractType == 0) {
				auto* missionOfferComponent = static_cast<MissionOfferComponent*>(interactedObject->GetComponent(eReplicaComponentType::MISSION_OFFER));

				if (missionOfferComponent != nullptr) {
					missionOfferComponent->OfferMissions(&entity, multiInteractID);
				}
			} else {
				interactedObject->OnUse(&entity);
			}
		} else {
			interactedObject->OnUse(&entity);
		}

		RequestUseEvent event(*this);
		interactedObject->HandleMsg(event);

		//Perform use task if possible:
		auto missionComponent = entity.GetComponent<MissionComponent>();

		if (!missionComponent) return;

		missionComponent->Progress(eMissionTaskType::TALK_TO_NPC, interactedObject->GetLOT(), interactedObject->GetObjectID());
		missionComponent->Progress(eMissionTaskType::INTERACT, interactedObject->GetLOT(), interactedObject->GetObjectID());
	}

	void RequestServerObjectInfo::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bVerbose);
		bitStream.Write(clientId);
		bitStream.Write(targetForReport);
	}

	bool RequestServerObjectInfo::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bVerbose));
		VALIDATE_READ(bitStream.Read(clientId));
		VALIDATE_READ(bitStream.Read(targetForReport));
		return true;
	}

	void RequestServerObjectInfo::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* handlingEntity = Game::entityManager->GetEntity(targetForReport);
		if (handlingEntity) {
			RequestServerObjectInfoEvent event(*this);
			handlingEntity->HandleMsg(event);
		} else LOG("Failed to find target %llu", targetForReport);
	}
}
