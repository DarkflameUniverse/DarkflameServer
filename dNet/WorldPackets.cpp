#include "WorldPackets.h"
#include "dCommonVars.h"
#include "BitStream.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "Game.h"
#include "LDFFormat.h"
#include "dServer.h"
#include "ZCompression.h"
#include "eConnectionType.h"
#include "BitStreamUtils.h"

#include <iostream>

void WorldPackets::SendLoadStaticZone(const SystemAddress& sysAddr, float x, float y, float z, uint32_t checksum, LWOZONEID zone) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::LOAD_STATIC_ZONE);

	bitStream.Write<uint16_t>(zone.GetMapID());
	bitStream.Write<uint16_t>(zone.GetInstanceID());
	//bitStream.Write<uint32_t>(zone.GetCloneID());
	bitStream.Write(0);

	bitStream.Write(checksum);
	bitStream.Write<uint16_t>(0);     // ??

	bitStream.Write(x);
	bitStream.Write(y);
	bitStream.Write(z);

	bitStream.Write<uint32_t>(0);     // Change this to eventually use 4 on activity worlds

	SEND_PACKET;
}

void WorldPackets::SendCharacterCreationResponse(const SystemAddress& sysAddr, eCharacterCreationResponse response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::CHARACTER_CREATE_RESPONSE);
	bitStream.Write(response);
	SEND_PACKET;
}

void WorldPackets::SendCharacterRenameResponse(const SystemAddress& sysAddr, eRenameResponse response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::CHARACTER_RENAME_RESPONSE);
	bitStream.Write(response);
	SEND_PACKET;
}

void WorldPackets::SendCharacterDeleteResponse(const SystemAddress& sysAddr, bool response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::DELETE_CHARACTER_RESPONSE);
	bitStream.Write<uint8_t>(response);
	SEND_PACKET;
}

void WorldPackets::SendTransferToWorld(const SystemAddress& sysAddr, const std::string& serverIP, uint32_t serverPort, bool mythranShift) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::TRANSFER_TO_WORLD);

	bitStream.Write(LUString(serverIP));
	bitStream.Write<uint16_t>(serverPort);
	bitStream.Write<uint8_t>(mythranShift);

	SEND_PACKET;
}

void WorldPackets::SendServerState(const SystemAddress& sysAddr) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::SERVER_STATES);
	bitStream.Write<uint8_t>(1); //If the server is receiving this request, it probably is ready anyway.
	SEND_PACKET;
}

void WorldPackets::SendCreateCharacter(const SystemAddress& sysAddr, int64_t reputation, LWOOBJID player, const std::string& xmlData, const std::u16string& username, eGameMasterLevel gm) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::CREATE_CHARACTER);

	RakNet::BitStream data;
	data.Write<uint32_t>(7); //LDF key count

	std::unique_ptr<LDFData<LWOOBJID>> objid(new LDFData<LWOOBJID>(u"objid", player));
	std::unique_ptr<LDFData<LOT>> lot(new LDFData<LOT>(u"template", 1));
	std::unique_ptr<LDFData<std::string>> xmlConfigData(new LDFData<std::string>(u"xmlData", xmlData));
	std::unique_ptr<LDFData<std::u16string>> name(new LDFData<std::u16string>(u"name", username));
	std::unique_ptr<LDFData<int32_t>> gmlevel(new LDFData<int32_t>(u"gmlevel", static_cast<int32_t>(gm)));
	std::unique_ptr<LDFData<int32_t>> chatmode(new LDFData<int32_t>(u"chatmode", static_cast<int32_t>(gm)));
	std::unique_ptr<LDFData<int64_t>> reputationLdf(new LDFData<int64_t>(u"reputation", reputation));

	objid->WriteToPacket(data);
	lot->WriteToPacket(data);
	name->WriteToPacket(data);
	gmlevel->WriteToPacket(data);
	chatmode->WriteToPacket(data);
	xmlConfigData->WriteToPacket(data);
	reputationLdf->WriteToPacket(data);

	//Compress the data before sending:
    const uint32_t reservedSize = ZCompression::GetMaxCompressedLength(data.GetNumberOfBytesUsed());
    uint8_t* compressedData = new uint8_t[reservedSize];

	// TODO There should be better handling here for not enough memory...
	if (!compressedData) return;

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

	SEND_PACKET;
	delete[] compressedData;
	LOG("Sent CreateCharacter for ID: %llu", player);
}

void WorldPackets::SendChatModerationResponse(const SystemAddress& sysAddr, bool requestAccepted, uint32_t requestID, const std::string& receiver, std::vector<std::pair<uint8_t, uint8_t>> unacceptedItems) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::CHAT_MODERATION_STRING);

	bitStream.Write<uint8_t>(unacceptedItems.empty()); // Is sentence ok?
	bitStream.Write<uint16_t>(0x16); // Source ID, unknown

	bitStream.Write<uint8_t>(requestID); // request ID
	bitStream.Write<char>(0); // chat mode

	bitStream.Write(LUWString(receiver, 42)); // receiver name

	for (auto it : unacceptedItems) {
		bitStream.Write<uint8_t>(it.first); // start index
		bitStream.Write<uint8_t>(it.second); // length
	}

	for (int i = unacceptedItems.size(); 64 > i; i++) {
		bitStream.Write<uint16_t>(0);
	}

	SEND_PACKET;
}

void WorldPackets::SendGMLevelChange(const SystemAddress& sysAddr, bool success, eGameMasterLevel highestLevel, eGameMasterLevel prevLevel, eGameMasterLevel newLevel) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, eConnectionType::CLIENT, eClientMessageType::MAKE_GM_RESPONSE);

	bitStream.Write<uint8_t>(success);
	bitStream.Write(static_cast<uint16_t>(highestLevel));
	bitStream.Write(static_cast<uint16_t>(prevLevel));
	bitStream.Write(static_cast<uint16_t>(newLevel));

	SEND_PACKET;
}
