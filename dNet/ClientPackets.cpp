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
#include "Logger.h"
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
#include "HavokVehiclePhysicsComponent.h"
#include "dConfig.h"
#include "CharacterComponent.h"
#include "Database.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "CheatDetection.h"
#include "Amf3.h"

void ClientPackets::HandleChatMessage(const SystemAddress& sysAddr, Packet* packet) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) {
		LOG("Unable to get user to parse chat message");
		return;
	}

	if (user->GetIsMuted()) {
		user->GetLastUsedChar()->SendMuteNotice();
		return;
	}

	CINSTREAM_SKIP_HEADER;

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
	bool isMythran = user->GetLastUsedChar()->GetGMLevel() > eGameMasterLevel::CIVILIAN;
	bool isOk = Game::chatFilter->IsSentenceOkay(GeneralUtils::UTF16ToWTF8(message), user->GetLastUsedChar()->GetGMLevel()).empty();
	LOG_DEBUG("Msg: %s was approved previously? %i", GeneralUtils::UTF16ToWTF8(message).c_str(), user->GetLastChatMessageApproved());
	if (!isOk) {
		// Add a limit to the string converted by general utils because it is a user received string and may be a bad actor.
		CheatDetection::ReportCheat(
			user,
			sysAddr,
			"Player %s attempted to bypass chat filter with message: %s",
			playerName.c_str(),
			GeneralUtils::UTF16ToWTF8(message, 512).c_str());
	}
	if (!isOk && !isMythran) return;

	std::string sMessage = GeneralUtils::UTF16ToWTF8(message);
	LOG("%s: %s", playerName.c_str(), sMessage.c_str());
	ChatPackets::SendChatMessage(sysAddr, chatChannel, playerName, user->GetLoggedInChar(), isMythran, message);
}

void ClientPackets::HandleClientPositionUpdate(const SystemAddress& sysAddr, Packet* packet) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) {
		LOG("Unable to get user to parse position update");
		return;
	}

	CINSTREAM_SKIP_HEADER;

	Entity* entity = Game::entityManager->GetEntity(user->GetLastUsedChar()->GetObjectID());
	if (!entity) return;

	ControllablePhysicsComponent* comp = static_cast<ControllablePhysicsComponent*>(entity->GetComponent(eReplicaComponentType::CONTROLLABLE_PHYSICS));
	if (!comp) return;

	/*
	//If we didn't move, this will match and stop our velocity
	if (packet->length == 37) {
		NiPoint3 zeroVel(0.0f, 0.0f, 0.0f);
		comp->SetVelocity(zeroVel);
		comp->SetAngularVelocity(zeroVel);
		comp->SetIsOnGround(true); //probably8
		Game::entityManager->SerializeEntity(entity);
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

	// TODO figure out how to use these. Ignoring for now, but reading in if they exist.
	bool hasLocalSpaceInfo{};
	LWOOBJID objectId{};
	NiPoint3 localSpacePosition{};
	bool hasLinearVelocity{};
	NiPoint3 linearVelocity{};
	if (inStream.Read(hasLocalSpaceInfo) && hasLocalSpaceInfo) {
		inStream.Read(objectId);
		inStream.Read(localSpacePosition.x);
		inStream.Read(localSpacePosition.y);
		inStream.Read(localSpacePosition.z);
		if (inStream.Read(hasLinearVelocity) && hasLinearVelocity) {
			inStream.Read(linearVelocity.x);
			inStream.Read(linearVelocity.y);
			inStream.Read(linearVelocity.z);
		}
	}
	bool hasRemoteInputInfo{};
	RemoteInputInfo remoteInput{};

	if (inStream.Read(hasRemoteInputInfo) && hasRemoteInputInfo) {
		inStream.Read(remoteInput.m_RemoteInputX);
		inStream.Read(remoteInput.m_RemoteInputY);
		inStream.Read(remoteInput.m_IsPowersliding);
		inStream.Read(remoteInput.m_IsModified);
	}

	bool updateChar = true;

	if (possessorComponent != nullptr) {
		auto* possassableEntity = Game::entityManager->GetEntity(possessorComponent->GetPossessable());

		if (possassableEntity != nullptr) {
			auto* possessableComponent = possassableEntity->GetComponent<PossessableComponent>();
			if (possessableComponent) {
				// While possessing something, only update char if we are attached to the thing we are possessing
				if (possessableComponent->GetPossessionType() != ePossessionType::ATTACHED_VISIBLE) updateChar = false;
			}

			auto* havokVehiclePhysicsComponent = possassableEntity->GetComponent<HavokVehiclePhysicsComponent>();
			if (havokVehiclePhysicsComponent != nullptr) {
				havokVehiclePhysicsComponent->SetPosition(position);
				havokVehiclePhysicsComponent->SetRotation(rotation);
				havokVehiclePhysicsComponent->SetIsOnGround(onGround);
				havokVehiclePhysicsComponent->SetIsOnRail(onRail);
				havokVehiclePhysicsComponent->SetVelocity(velocity);
				havokVehiclePhysicsComponent->SetDirtyVelocity(velocityFlag);
				havokVehiclePhysicsComponent->SetAngularVelocity(angVelocity);
				havokVehiclePhysicsComponent->SetDirtyAngularVelocity(angVelocityFlag);
				havokVehiclePhysicsComponent->SetRemoteInputInfo(remoteInput);
			} else {
				// Need to get the mount's controllable physics
				auto* controllablePhysicsComponent = possassableEntity->GetComponent<ControllablePhysicsComponent>();
				if (!controllablePhysicsComponent) return;
				controllablePhysicsComponent->SetPosition(position);
				controllablePhysicsComponent->SetRotation(rotation);
				controllablePhysicsComponent->SetIsOnGround(onGround);
				controllablePhysicsComponent->SetIsOnRail(onRail);
				controllablePhysicsComponent->SetVelocity(velocity);
				controllablePhysicsComponent->SetDirtyVelocity(velocityFlag);
				controllablePhysicsComponent->SetAngularVelocity(angVelocity);
				controllablePhysicsComponent->SetDirtyAngularVelocity(angVelocityFlag);
			}
			Game::entityManager->SerializeEntity(possassableEntity);
		}
	}

	if (!updateChar) {
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
	Game::entityManager->QueueGhostUpdate(player->GetObjectID());

	if (updateChar) Game::entityManager->SerializeEntity(entity);

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

		Game::entityManager->SerializeEntity(entity, player);
	}
	*/
}

void ClientPackets::HandleChatModerationRequest(const SystemAddress& sysAddr, Packet* packet) {
	User* user = UserManager::Instance()->GetUser(sysAddr);
	if (!user) {
		LOG("Unable to get user to parse chat moderation request");
		return;
	}

	auto* entity = Player::GetPlayer(sysAddr);

	if (entity == nullptr) {
		LOG("Unable to get player to parse chat moderation request");
		return;
	}

	// Check if the player has restricted chat access
	auto* character = entity->GetCharacter();

	if (character->HasPermission(ePermissionMap::RestrictedChatAccess)) {
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
			auto characterIdFetch = Database::Get()->GetCharacterInfo(receiver);

			if (characterIdFetch) {
				idOfReceiver = characterIdFetch->id;
			}
		}
		const auto& bffMap = user->GetIsBestFriendMap();
		if (bffMap.find(receiver) == bffMap.end() && idOfReceiver != LWOOBJID_EMPTY) {
			auto bffInfo = Database::Get()->GetBestFriendStatus(entity->GetObjectID(), idOfReceiver);

			if (bffInfo) {
				isBestFriend = bffInfo->bestFriendStatus == 3;
			}

			if (isBestFriend) {
				user->UpdateBestFriendValue(receiver, true);
			}
		} else if (bffMap.find(receiver) != bffMap.end()) {
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

void ClientPackets::SendTop5HelpIssues(Packet* packet) {
	auto* user = UserManager::Instance()->GetUser(packet->systemAddress);
	if (!user) return;
	auto* character = user->GetLastUsedChar();
	if (!character) return;
	auto * entity = character->GetEntity();
	if (!entity) return;

	CINSTREAM_SKIP_HEADER;
	int32_t language = 0;
	inStream.Read(language);

	// TODO: Handle different languages in a nice way
	// 0: en_US
	// 1: pl_US
	// 2: de_DE
	// 3: en_GB

	AMFArrayValue data;
	// Summaries
	data.Insert("Summary0", Game::config->GetValue("help_0_summary"));
	data.Insert("Summary1", Game::config->GetValue("help_1_summary"));
	data.Insert("Summary2", Game::config->GetValue("help_2_summary"));
	data.Insert("Summary3", Game::config->GetValue("help_3_summary"));
	data.Insert("Summary4", Game::config->GetValue("help_4_summary"));

	// Descriptions
	data.Insert("Description0", Game::config->GetValue("help_0_description"));
	data.Insert("Description1", Game::config->GetValue("help_1_description"));
	data.Insert("Description2", Game::config->GetValue("help_2_description"));
	data.Insert("Description3", Game::config->GetValue("help_3_description"));
	data.Insert("Description4", Game::config->GetValue("help_4_description"));
	
	GameMessages::SendUIMessageServerToSingleClient(entity, packet->systemAddress, "UIHelpTop5", data);
	
}
