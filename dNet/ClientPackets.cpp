/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
#include "dCommonVars.h"
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
