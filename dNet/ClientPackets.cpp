/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
#include "dCommonVars.h"
#include "PositionUpdate.h"
#include "eLoginResponse.h"

namespace ClientPackets {
	void LoginResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(responseCode);
		for (const auto& event : events) bitStream.Write(event);
		bitStream.Write(versionMajor);
		bitStream.Write(versionCurrent);
		bitStream.Write(versionMinor);
		bitStream.Write(userKey);
		bitStream.Write(worldServerIP);
		bitStream.Write(chatServerIP);
		bitStream.Write(worldServerPort);
		bitStream.Write(chatServerPort);
		bitStream.Write(cdnKey);
		bitStream.Write(cdnTicket);
		bitStream.Write(language);
		bitStream.Write(localization);
		bitStream.Write<uint8_t>(justUpgradedFromF2P);
		bitStream.Write<uint8_t>(isFreeToPlay);
		bitStream.Write(freeToPlayTimeRemaining);
		bitStream.Write<uint16_t>(errorMessage.length());
		bitStream.Write(LUWString(errorMessage, static_cast<uint32_t>(errorMessage.length())));
		stamps.Serialize(bitStream);
	}

	bool LoginResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(responseCode));
		for (auto& event : events) VALIDATE_READ(bitStream.Read(event));
		VALIDATE_READ(bitStream.Read(versionMajor));
		VALIDATE_READ(bitStream.Read(versionCurrent));
		VALIDATE_READ(bitStream.Read(versionMinor));
		VALIDATE_READ(bitStream.Read(userKey));
		VALIDATE_READ(bitStream.Read(worldServerIP));
		VALIDATE_READ(bitStream.Read(chatServerIP));
		VALIDATE_READ(bitStream.Read(worldServerPort));
		VALIDATE_READ(bitStream.Read(chatServerPort));
		VALIDATE_READ(bitStream.Read(cdnKey));
		VALIDATE_READ(bitStream.Read(cdnTicket));
		VALIDATE_READ(bitStream.Read(language));
		VALIDATE_READ(bitStream.Read(localization));
		uint8_t flag{};
		VALIDATE_READ(bitStream.Read(flag));
		justUpgradedFromF2P = flag != 0;
		VALIDATE_READ(bitStream.Read(flag));
		isFreeToPlay = flag != 0;
		VALIDATE_READ(bitStream.Read(freeToPlayTimeRemaining));
		uint16_t errorLength{};
		VALIDATE_READ(bitStream.Read(errorLength));
		LUWString error(errorLength);
		if (errorLength > 0) VALIDATE_READ(bitStream.Read(error)); // RakNet fails reads of 0 bits
		errorMessage = error.GetAsString();
		VALIDATE_READ(stamps.Deserialize(bitStream));
		return true;
	}
}

ChatMessage ClientPackets::HandleChatMessage(Packet* packet) {
	CINSTREAM_SKIP_HEADER;

	ChatMessage message;
	int32_t messageLength{};

	inStream.Read(message.chatChannel);
	inStream.Read(message.unknown);
	inStream.Read(messageLength);

	if (messageLength > MAX_MESSAGE_LENGTH || messageLength < 0) return message;

	for (int32_t i = 0; i < (messageLength - 1); ++i) {
		char16_t character;
		inStream.Read(character);
		message.message.push_back(character);
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
	if (messageLength > MAX_MESSAGE_LENGTH) return request;
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
