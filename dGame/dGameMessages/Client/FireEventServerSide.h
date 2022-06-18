#pragma once

#include "../GameMessage.h"

#include "EntityManager.h"
#include "Player.h"
#include "RocketLaunchpadControlComponent.h"
#include "ZoneInstanceManager.h"
#include "WorldPackets.h"
#include "Character.h"
#include "dLogger.h"
#include "dZoneManager.h"
#include "GameMessages.h"
#include "AMFFormat.h"

class FireEventServerSide : public GameMessage {
public:
	constexpr static GAME_MSG GetId() { return GAME_MSG_FIRE_EVENT_SERVER_SIDE; }

	uint32_t argsLength{};
	std::u16string args{};
	bool param1IsDefault{};
	int param1 = -1;
	bool param2IsDefault{};
	int param2 = -1;
	bool param3IsDefault{};
	int param3 = -1;
	LWOOBJID senderID{};

	void Deserialize(RakNet::BitStream* inStream) override {
		inStream->Read(argsLength);
		for (uint32_t i = 0; i < argsLength; ++i) {
			uint16_t character;
			inStream->Read(character);
			args.push_back(character);
		}
		inStream->Read(param1IsDefault);
		if (param1IsDefault) inStream->Read(param1);
		inStream->Read(param2IsDefault);
		if (param2IsDefault) inStream->Read(param2);
		inStream->Read(param3IsDefault);
		if (param3IsDefault) inStream->Read(param3);
		inStream->Read(senderID);
	}

	void Handle() override {
		auto* sender = EntityManager::Instance()->GetEntity(senderID);
		auto* player = Player::GetPlayer(sysAddr);

		if (!player) {
			return;
		}

		if (args == u"toggleMail") {
			AMFFalseValue* value = new AMFFalseValue();

			AMFArrayValue args;
			args.InsertValue("visible", value);
			GameMessages::SendUIMessageServerToSingleClient(associate, sysAddr, "ToggleMail", &args);
			delete value;
		}

		// This should probably get it's own "ServerEvents" system or something at some point
		if (args == u"ZonePlayer") {
			// Should probably check to make sure they're using a launcher at some point before someone makes a hack that lets you testmap

			LWOCLONEID cloneId = 0;
			LWOMAPID mapId = 0;

			auto* rocketPad = associate->GetComponent<RocketLaunchpadControlComponent>();

			if (rocketPad == nullptr) return;

			cloneId = rocketPad->GetSelectedCloneId(player->GetObjectID());

			if (param2) {
				mapId = rocketPad->GetDefaultZone();
			}
			else {
				mapId = param3;
			}

			if (mapId == 0)
			{
				mapId = rocketPad->GetSelectedMapId(player->GetObjectID());
			}

			if (mapId == 0)
			{
				mapId = dZoneManager::Instance()->GetZoneID().GetMapID(); // Fallback to sending the player back to the same zone.
			}

			Game::logger->Log("FireEventServerSide", "Player %llu has requested zone transfer to (%i, %i).\n", sender->GetObjectID(), (int)mapId, (int)cloneId);

			auto* character = player->GetCharacter();

			if (mapId <= 0) {
				return;
			}

			ZoneInstanceManager::Instance()->RequestZoneTransfer(Game::server, mapId, cloneId, false, [=](bool mythranShift, uint32_t zoneID, uint32_t zoneInstance, uint32_t zoneClone, std::string serverIP, uint16_t serverPort) {
				Game::logger->Log("UserManager", "Transferring %s to Zone %i (Instance %i | Clone %i | Mythran Shift: %s) with IP %s and Port %i\n", character->GetName().c_str(), zoneID, zoneInstance, zoneClone, mythranShift == true ? "true" : "false", serverIP.c_str(), serverPort);

				if (character) {
					character->SetZoneID(zoneID);
					character->SetZoneInstance(zoneInstance);
					character->SetZoneClone(zoneClone);
				}

				WorldPackets::SendTransferToWorld(sysAddr, serverIP, serverPort, mythranShift);
				return;
			});
		}

		associate->OnFireEventServerSide(sender, GeneralUtils::UTF16ToWTF8(args), param1, param2, param3);
	}
};