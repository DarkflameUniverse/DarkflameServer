/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
#include "dCommonVars.h"
#include "PositionUpdate.h"
#include "LDFFormat.h"
#include "ZCompression.h"

namespace ClientPackets {
	void LoadStaticZone::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint16_t>(zoneID.GetMapID());
		bitStream.Write<uint16_t>(zoneID.GetInstanceID());
		bitStream.Write<uint32_t>(checksum);
		bitStream.Write<uint8_t>(editorEnabled);
		bitStream.Write<uint8_t>(editorLevel);
		bitStream.Write(position.x);
		bitStream.Write(position.y);
		bitStream.Write(position.z);
		bitStream.Write<uint32_t>(instanceType);
	}
	void CharacterCreationResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write(response);
	}
	
	void CharacterRenameResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write(response);
	}

	void CharacterDeleteResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(success);
	}

	void TransferToWorld::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write(LUString(serverIP));
		bitStream.Write<uint16_t>(serverPort);
		bitStream.Write<uint8_t>(mythranShift);
	}

	void ServerState::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(serverReady);
	}
	void CreateCharacter::Serialize(RakNet::BitStream &bitStream) const {

		RakNet::BitStream data;

		data.Write<uint32_t>(7); //LDF key count
		std::unique_ptr<LDFData<LWOOBJID>> objidLDF(new LDFData<LWOOBJID>(u"objid", objid));
		objidLDF->WriteToPacket(data);

		std::unique_ptr<LDFData<LOT>> templateIDLDF(new LDFData<LOT>(u"template", templateID));
		templateIDLDF->WriteToPacket(data);

		std::unique_ptr<LDFData<std::u16string>> nameLDF(new LDFData<std::u16string>(u"name", name));
		nameLDF->WriteToPacket(data);

		std::unique_ptr<LDFData<int32_t>> gmlevelLDF(new LDFData<int32_t>(u"gmlevel", static_cast<int32_t>(gmLevel)));
		gmlevelLDF->WriteToPacket(data);

		std::unique_ptr<LDFData<int32_t>> chatModeLDF(new LDFData<int32_t>(u"chatmode", static_cast<int32_t>(chatMode)));
		chatModeLDF->WriteToPacket(data);

		std::unique_ptr<LDFData<std::string>> xmlConfigData(new LDFData<std::string>(u"xmlData", xmlData));
		xmlConfigData->WriteToPacket(data);

		std::unique_ptr<LDFData<int64_t>> reputationLdf(new LDFData<int64_t>(u"reputation", reputation));
		reputationLdf->WriteToPacket(data);

		//Compress the data before sending:
		const uint32_t reservedSize = ZCompression::GetMaxCompressedLength(data.GetNumberOfBytesUsed());
		uint8_t* compressedData = new uint8_t[reservedSize];

		if (!compressedData) {
			throw std::runtime_error("Failed to allocate memory for compressed data");
		}

		size_t size = ZCompression::Compress(data.GetData(), data.GetNumberOfBytesUsed(), compressedData, reservedSize);

		assert(size <= reservedSize);

		bitStream.Write<uint32_t>(size + 9); //size of data + header bytes (8)
		bitStream.Write<uint8_t>(1);         //compressed boolean, true
		bitStream.Write<uint32_t>(data.GetNumberOfBytesUsed());
		bitStream.Write<uint32_t>(size);

		/**
		 * In practice, this warning serves no purpose for us.  We allocate the max memory needed on the heap
		 * and then compress the data.  In the off chance that the compression actually increases the size,
		 * an assertion is done to prevent bad data from being saved or sent.
		 */
	#pragma warning(disable:6385) // C6385 Reading invalid data from 'compressedData'.
		bitStream.WriteAlignedBytes(compressedData, size);
	#pragma warning(default:6385)
		delete[] compressedData;
	};

	void ChatModerationString::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(rejectedWords.empty()); // Accepted
		bitStream.Write<uint16_t>(0); // padding

		bitStream.Write(chatChannel);
		bitStream.Write(chatMode);

		bitStream.Write(LUWString(receiver, 42));

		for (auto it : rejectedWords) {
			bitStream.Write<uint8_t>(it.first); // start index
			bitStream.Write<uint8_t>(it.second); // length
		}

		// Pad out the rest of the packet
		// The client expects 64 items, so we need to write 64 - rejectedWords.size() empty items
		for (int i = rejectedWords.size(); 64 > i; i++) {
			bitStream.Write<uint16_t>(0);
		}

	}

	void GMLevelChange::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint8_t>(success);
		bitStream.Write(static_cast<uint16_t>(highestLevel));
		bitStream.Write(static_cast<uint16_t>(prevLevel));
		bitStream.Write(static_cast<uint16_t>(newLevel));
	}

	void DebugOutput::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint32_t>(data.size());
		bitStream.Write(data);
	}

	void HTTPMonitorInfoResponse::Serialize(RakNet::BitStream &bitStream) const {
		bitStream.Write<uint16_t>(port);
		bitStream.Write<uint8_t>(openWeb);
		bitStream.Write<uint8_t>(supportsSum);
		bitStream.Write<uint8_t>(supportsDetail);
		bitStream.Write<uint8_t>(supportsWho);
		bitStream.Write<uint8_t>(supportsObjects);
	}
}

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
