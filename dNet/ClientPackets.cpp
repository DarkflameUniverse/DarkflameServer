/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
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
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "CheatDetection.h"
#include "Recorder.h"
#include "PositionUpdate.h"

ChatMessage ClientPackets::HandleChatMessage(Packet* packet) {
	CINSTREAM_SKIP_HEADER;

	ChatMessage message;
	uint32_t messageLength;

	inStream.Read(message.chatChannel);
	inStream.Read(message.unknown);
	inStream.Read(messageLength);

	for (uint32_t i = 0; i < (messageLength - 1); ++i) {
		uint16_t character;
		inStream.Read(character);
		message.message.push_back(character);
	}

	std::string sMessage = GeneralUtils::UTF16ToWTF8(message);
	LOG("%s: %s", playerName.c_str(), sMessage.c_str());
	ChatPackets::SendChatMessage(sysAddr, chatChannel, playerName, user->GetLoggedInChar(), isMythran, message);

	auto* recorder = Cinema::Recording::Recorder::GetRecorder(user->GetLoggedInChar());

	if (recorder != nullptr) {
		recorder->AddRecord(new Cinema::Recording::SpeakRecord(sMessage));
	}
	
	return message;
}

PositionUpdate ClientPackets::HandleClientPositionUpdate(Packet* packet) {
	PositionUpdate update;
	CINSTREAM_SKIP_HEADER;

	inStream.Read(update.position.x);
	inStream.Read(update.position.y);
	inStream.Read(update.position.z);

	inStream.Read(update.rotation.x);
	inStream.Read(update.rotation.y);
	inStream.Read(update.rotation.z);
	inStream.Read(update.rotation.w);

	inStream.Read(update.onGround);
	inStream.Read(update.onRail);

	bool velocityFlag = false;
	inStream.Read(velocityFlag);
	if (velocityFlag) {
		inStream.Read(update.velocity.x);
		inStream.Read(update.velocity.y);
		inStream.Read(update.velocity.z);
	}

	bool angVelocityFlag = false;
	inStream.Read(angVelocityFlag);
	if (angVelocityFlag) {
		inStream.Read(update.angularVelocity.x);
		inStream.Read(update.angularVelocity.y);
		inStream.Read(update.angularVelocity.z);
	}

	// TODO figure out how to use these. Ignoring for now, but reading in if they exist.
	bool hasLocalSpaceInfo{};
	if (inStream.Read(hasLocalSpaceInfo) && hasLocalSpaceInfo) {
		inStream.Read(update.localSpaceInfo.objectId);
		inStream.Read(update.localSpaceInfo.position.x);
		inStream.Read(update.localSpaceInfo.position.y);
		inStream.Read(update.localSpaceInfo.position.z);
		bool hasLinearVelocity = false;
		if (inStream.Read(hasLinearVelocity) && hasLinearVelocity) {
			inStream.Read(update.localSpaceInfo.linearVelocity.x);
			inStream.Read(update.localSpaceInfo.linearVelocity.y);
			inStream.Read(update.localSpaceInfo.linearVelocity.z);
		}
	}

	bool hasRemoteInputInfo{};
	if (inStream.Read(hasRemoteInputInfo) && hasRemoteInputInfo) {
		inStream.Read(update.remoteInputInfo.m_RemoteInputX);
		inStream.Read(update.remoteInputInfo.m_RemoteInputY);
		inStream.Read(update.remoteInputInfo.m_IsPowersliding);
		inStream.Read(update.remoteInputInfo.m_IsModified);
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

			auto* vehiclePhysicsComponent = possassableEntity->GetComponent<VehiclePhysicsComponent>();
			if (vehiclePhysicsComponent != nullptr) {
				vehiclePhysicsComponent->SetPosition(position);
				vehiclePhysicsComponent->SetRotation(rotation);
				vehiclePhysicsComponent->SetIsOnGround(onGround);
				vehiclePhysicsComponent->SetIsOnRail(onRail);
				vehiclePhysicsComponent->SetVelocity(velocity);
				vehiclePhysicsComponent->SetDirtyVelocity(velocityFlag);
				vehiclePhysicsComponent->SetAngularVelocity(angVelocity);
				vehiclePhysicsComponent->SetDirtyAngularVelocity(angVelocityFlag);
				vehiclePhysicsComponent->SetRemoteInputInfo(remoteInput);
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

	auto* recorder = Cinema::Recording::Recorder::GetRecorder(entity->GetObjectID());

	if (recorder != nullptr) {
		recorder->AddRecord(new Cinema::Recording::MovementRecord(
			position,
			rotation,
			velocity,
			angVelocity,
			onGround,
			velocityFlag,
			angVelocityFlag
		));
	}

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
	return update;
}

ChatModerationRequest ClientPackets::HandleChatModerationRequest(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	
	ChatModerationRequest request;

	inStream.Read(request.chatLevel);
	inStream.Read(request.requestID);

	for (uint32_t i = 0; i < 42; ++i) {
		uint16_t character;
		inStream.Read(character);
		request.receiver.push_back(static_cast<uint8_t>(character));
	}

	if (!request.receiver.empty()) {
		if (std::string(request.receiver.c_str(), 4) == "[GM]") { // Shift the string forward if we are speaking to a GM as the client appends "[GM]" if they are
			request.receiver = std::string(request.receiver.c_str() + 4, request.receiver.size() - 4);
		}
	}

	uint16_t messageLength;
	inStream.Read(messageLength);
	for (uint32_t i = 0; i < messageLength; ++i) {
		uint16_t character;
		inStream.Read(character);
		request.message.push_back(static_cast<uint8_t>(character));
	}

	return request;
}

int32_t ClientPackets::SendTop5HelpIssues(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	int32_t language = 0;
	inStream.Read(language);
	return language;
}
