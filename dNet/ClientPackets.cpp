/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
#include "UserManager.h"
#include "User.h"
#include "Character.h"
#include "EntityManager.h"
#include "Entity.h"
#include "ControllablePhysicsComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "WorldPackets.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "dCommonVars.h"
#include "BitStream.h"
#include "dChatFilter.h"
#include "WorldPackets.h"
#include "ChatPackets.h"
#include "dServer.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "Player.h"
#include "Zone.h"
#include "PossessorComponent.h"
#include "PossessableComponent.h"
#include "VehiclePhysicsComponent.h"
#include "dConfig.h"
#include "CharacterComponent.h"
#include "Database.h"



void ClientPackets::HandleChatMessage(const SystemAddress& sysAddr, Packet* packet) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) {
		Game::logger->Log("ClientPackets", "Unable to get user to parse chat message\n");
		return;
	}

	if (user->GetIsMuted()) {
		user->GetLastUsedChar()->SendMuteNotice();
		return;
	}

	CINSTREAM;
	uint64_t header;
	inStream.Read(header);

	char chatChannel;
	uint16_t unknown;
	uint32_t messageLength;
	std::u16string message;

	inStream.Read(chatChannel);
	inStream.Read(unknown);
	inStream.Read(messageLength);

	for (uint32_t i = 0; i < (messageLength - 1); ++i) {
		uint16_t character;
		inStream.Read(character);
		message.push_back(character);
	}

	std::string playerName = user->GetLastUsedChar()->GetName();
	bool isMythran = user->GetLastUsedChar()->GetGMLevel() > 0;

	if (!user->GetLastChatMessageApproved() && !isMythran) return;

	std::string sMessage = GeneralUtils::UTF16ToWTF8(message);
	Game::logger->Log("Chat", "%s: %s\n", playerName.c_str(), sMessage.c_str());
	ChatPackets::SendChatMessage(sysAddr, chatChannel, playerName, user->GetLoggedInChar(), isMythran, message);
}

void ClientPackets::HandleClientPositionUpdate(const SystemAddress& sysAddr, Packet* packet) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) {
		Game::logger->Log("ClientPackets", "Unable to get user to parse position update\n");
		return;
	}

	CINSTREAM;
	uint64_t header;
	inStream.Read(header);

	Entity* entity = EntityManager::Instance()->GetEntity(user->GetLastUsedChar()->GetObjectID());
	if (!entity) return;

	ControllablePhysicsComponent* comp = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(COMPONENT_TYPE_CONTROLLABLE_PHYSICS));
	if (!comp) return;

	/*
	//If we didn't move, this will match and stop our velocity
	if (packet->length == 37) {
		NiPoint3 zeroVel(0.0f, 0.0f, 0.0f);
		comp->SetVelocity(zeroVel);
		comp->SetAngularVelocity(zeroVel);
		comp->SetIsOnGround(true); //probably8
		EntityManager::Instance()->SerializeEntity(entity);
		return;
	}
	*/

	auto* possessorComponent = entity->GetComponent<PossessorComponent>();

	NiPoint3 position;
	inStream.Read(position.x);
	inStream.Read(position.y);
	inStream.Read(position.z);

	NiQuaternion rotation;
	inStream.Read(rotation.x);
	inStream.Read(rotation.y);
	inStream.Read(rotation.z);
	inStream.Read(rotation.w);

	bool onGround = false;
	bool onRail = false;
	inStream.Read(onGround);
	inStream.Read(onRail);

	bool velocityFlag = false;
	inStream.Read(velocityFlag);
	NiPoint3 velocity{};
	if (velocityFlag) {
		inStream.Read(velocity.x);
		inStream.Read(velocity.y);
		inStream.Read(velocity.z);
	}

	bool angVelocityFlag = false;
	inStream.Read(angVelocityFlag);
	NiPoint3 angVelocity{};
	if (angVelocityFlag) {
		inStream.Read(angVelocity.x);
		inStream.Read(angVelocity.y);
		inStream.Read(angVelocity.z);
	}

	bool hasVehicle = false;

	if (possessorComponent != nullptr) {
		auto* possassableEntity = EntityManager::Instance()->GetEntity(possessorComponent->GetPossessable());

		if (possassableEntity != nullptr) {
			auto* vehiclePhysicsComponent = possassableEntity->GetComponent<VehiclePhysicsComponent>();

			if (vehiclePhysicsComponent != nullptr) {
				// This is flipped for whatever reason
				rotation = NiQuaternion(rotation.z, rotation.y, rotation.x, rotation.w);

				vehiclePhysicsComponent->SetPosition(position);
				vehiclePhysicsComponent->SetRotation(rotation);
				vehiclePhysicsComponent->SetIsOnGround(onGround);
				vehiclePhysicsComponent->SetIsOnRail(onRail);
				vehiclePhysicsComponent->SetVelocity(velocity);
				vehiclePhysicsComponent->SetDirtyVelocity(velocityFlag);
				vehiclePhysicsComponent->SetAngularVelocity(angVelocity);
				vehiclePhysicsComponent->SetDirtyAngularVelocity(angVelocityFlag);

				EntityManager::Instance()->SerializeEntity(possassableEntity);

				hasVehicle = true;
			}
		}
	}

	if (hasVehicle) {
		velocity = NiPoint3::ZERO;
		angVelocity = NiPoint3::ZERO;
	}

	// Handle statistics
	auto* characterComponent = entity->GetComponent<CharacterComponent>();
	if (characterComponent != nullptr) {
	    characterComponent->TrackPositionUpdate(position);
	}

	comp->SetPosition(position);
	comp->SetRotation(rotation);
	comp->SetIsOnGround(onGround);
	comp->SetIsOnRail(onRail);
	comp->SetVelocity(velocity);
	comp->SetDirtyVelocity(velocityFlag);
	comp->SetAngularVelocity(angVelocity);
	comp->SetDirtyAngularVelocity(angVelocityFlag);

	auto* player = static_cast<Player*>(entity);
	player->SetGhostReferencePoint(position);
	EntityManager::Instance()->QueueGhostUpdate(player->GetObjectID());

	if (!hasVehicle) {
		EntityManager::Instance()->SerializeEntity(entity);
	}

	//TODO: add moving platform stuffs
	/*bool movingPlatformFlag;
	inStream.Read(movingPlatformFlag);
	if (movingPlatformFlag) {
		LWOOBJID objectID;
		NiPoint3 niData2;

		inStream.Read(objectID);
		inStream.Read(niData2.x);
		inStream.Read(niData2.y);
		inStream.Read(niData2.z);



		bool niData3Flag;
		inStream.Read(niData3Flag);
		if (niData3Flag) {
			NiPoint3 niData3;
			inStream.Read(niData3.x);
			inStream.Read(niData3.y);
			inStream.Read(niData3.z);

			controllablePhysics->GetLocationData()->GetMovingPlatformData()->SetData3(niData3);
		}
	}*/

	/*
	for (int i = 0; i < Game::server->GetReplicaManager()->GetParticipantCount(); ++i)
	{
		const auto& player = Game::server->GetReplicaManager()->GetParticipantAtIndex(i);

		if (entity->GetSystemAddress() == player)
		{
			continue;
		}

		EntityManager::Instance()->SerializeEntity(entity, player);
	}
	*/
}

void ClientPackets::HandleChatModerationRequest(const SystemAddress& sysAddr, Packet* packet) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) {
		Game::logger->Log("ClientPackets", "Unable to get user to parse chat moderation request\n");
		return;
	}

	auto* entity = Player::GetPlayer(sysAddr);

	if (entity == nullptr) {
		Game::logger->Log("ClientPackets", "Unable to get player to parse chat moderation request\n");
		return;
	}

	// Check if the player has restricted chat access
	auto* character = entity->GetCharacter();

	if (character->HasPermission(PermissionMap::RestrictedChatAccess))
	{
		// Send a message to the player
		ChatPackets::SendSystemMessage(
			sysAddr,
			u"This character has restricted chat access."
		);

		return;
	}

	RakNet::BitStream stream(packet->data, packet->length, false);

	uint64_t header;
	stream.Read(header);

	// Data
	uint8_t chatLevel;
	uint8_t requestID;
	uint16_t messageLength;

	std::string receiver = "";
	std::string message = "";

	stream.Read(chatLevel);
	stream.Read(requestID);

	for (uint32_t i = 0; i < 42; ++i) {
		uint16_t character;
		stream.Read(character);
		receiver.push_back(static_cast<uint8_t>(character));
	}

	if (!receiver.empty()) {
		if (std::string(receiver.c_str(), 4) == "[GM]") { // Shift the string forward if we are speaking to a GM as the client appends "[GM]" if they are
			receiver = std::string(receiver.c_str() + 4, receiver.size() - 4);
		}
	}

	stream.Read(messageLength);
	for (uint32_t i = 0; i < messageLength; ++i) {
		uint16_t character;
		stream.Read(character);
		message.push_back(static_cast<uint8_t>(character));
	}

	bool isBestFriend = false;

	if (chatLevel == 1) {
		// Private chat
		LWOOBJID idOfReceiver = LWOOBJID_EMPTY;

		{
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT name FROM charinfo WHERE name = ?");
			stmt->setString(1, receiver);

			sql::ResultSet* res = stmt->executeQuery();

			if (res->next()) {
				idOfReceiver = res->getInt("id");
			}

			delete stmt;
			delete res;
		}

		if (user->GetIsBestFriendMap().find(receiver) == user->GetIsBestFriendMap().end() && idOfReceiver != LWOOBJID_EMPTY) {
			sql::PreparedStatement* stmt = Database::CreatePreppedStmt("SELECT * FROM friends WHERE (player_id = ? AND friend_id = ?) OR (player_id = ? AND friend_id = ?) LIMIT 1;");
			stmt->setInt(1, entity->GetObjectID());
			stmt->setInt(2, idOfReceiver);
			stmt->setInt(3, idOfReceiver);
			stmt->setInt(4, entity->GetObjectID());

			sql::ResultSet* res = stmt->executeQuery();

			if (res->next()) {
				isBestFriend = res->getInt("best_friend") == 3;
			}

			if (isBestFriend) {
				auto tmpBestFriendMap = user->GetIsBestFriendMap();
				tmpBestFriendMap[receiver] = true;
				user->SetIsBestFriendMap(tmpBestFriendMap);
			}

			delete res;
			delete stmt;
		}
		else if (user->GetIsBestFriendMap().find(receiver) != user->GetIsBestFriendMap().end()) {
			isBestFriend = true;
		}
	}

	std::vector<std::pair<uint8_t, uint8_t>> segments = Game::chatFilter->IsSentenceOkay(message, entity->GetGMLevel(), !(isBestFriend && chatLevel == 1));

	bool bAllClean = segments.empty();

	if (user->GetIsMuted()) {
		bAllClean = false;
	}

	user->SetLastChatMessageApproved(bAllClean);
	WorldPackets::SendChatModerationResponse(sysAddr, bAllClean, requestID, receiver, segments);
}
