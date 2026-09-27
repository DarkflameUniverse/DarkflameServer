#ifndef WORLDPACKETSLEGACY_H
#define WORLDPACKETSLEGACY_H

// FROZEN ORACLE - DO NOT EDIT.
// Verbatim copies of the hand written code that WorldPackets / the ClientPackets world responses replaced, from
// before the conversion: every WorldPackets::Send* function and HTTPMonitorInfo (dNet/WorldPackets.cpp), the four
// ClientPackets parse functions (dNet/ClientPackets.cpp), and, below them, the parts of UserManager.cpp and
// WorldServer.cpp's HandlePacket that read or wrote packets inline (each copied verbatim into a function that
// returns what was read, or the packet that was built, instead of acting on it).
// Only the namespace changed. The byte-equality tests run the same inputs through these and the new structs.

#include "BitStreamUtils.h"
#include "dCommonVars.h"
#include "dServer.h"
#include "eCharacterCreationResponse.h"
#include "eFunnessTypes.h"
#include "eGameMasterLevel.h"
#include "eRenameResponse.h"
#include "Game.h"
#include "LDFFormat.h"
#include "Logger.h"
#include "MessageType/Chat.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"
#include "PositionUpdate.h"
#include "ServiceType.h"
#include "ZCompression.h"

#include <cassert>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <vector>

namespace LegacyWorldPackets {
struct HTTPMonitorInfo {
	uint16_t port = 80;
	bool openWeb = false;
	bool supportsSum = false;
	bool supportsDetail = false;
	bool supportsWho = false;
	bool supportsObjects = false;
	void Serialize(RakNet::BitStream &bitstream) const;
};

inline void HTTPMonitorInfo::Serialize(RakNet::BitStream& bitStream) const {
	bitStream.Write(port);
	bitStream.Write<uint8_t>(openWeb);
	bitStream.Write<uint8_t>(supportsSum);
	bitStream.Write<uint8_t>(supportsDetail);
	bitStream.Write<uint8_t>(supportsWho);
	bitStream.Write<uint8_t>(supportsObjects);
}

inline void SendLoadStaticZone(const SystemAddress& sysAddr, float x, float y, float z, uint32_t checksum, LWOZONEID zone) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::LOAD_STATIC_ZONE);

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

inline void SendCharacterCreationResponse(const SystemAddress& sysAddr, eCharacterCreationResponse response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::CHARACTER_CREATE_RESPONSE);
	bitStream.Write(response);
	SEND_PACKET;
}

inline void SendCharacterRenameResponse(const SystemAddress& sysAddr, eRenameResponse response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::CHARACTER_RENAME_RESPONSE);
	bitStream.Write(response);
	SEND_PACKET;
}

inline void SendCharacterDeleteResponse(const SystemAddress& sysAddr, bool response) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::DELETE_CHARACTER_RESPONSE);
	bitStream.Write<uint8_t>(response);
	SEND_PACKET;
}

inline void SendTransferToWorld(const SystemAddress& sysAddr, const std::string& serverIP, uint32_t serverPort, bool mythranShift) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::TRANSFER_TO_WORLD);

	bitStream.Write(LUString(serverIP));
	bitStream.Write<uint16_t>(serverPort);
	bitStream.Write<uint8_t>(mythranShift);

	SEND_PACKET;
}

inline void SendServerState(const SystemAddress& sysAddr) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::SERVER_STATES);
	bitStream.Write<uint8_t>(1); //If the server is receiving this request, it probably is ready anyway.
	SEND_PACKET;
}

inline void SendCreateCharacter(const SystemAddress& sysAddr, int64_t reputation, LWOOBJID player, const std::string& xmlData, const std::u16string& username, eGameMasterLevel gm, const LWOCLONEID cloneID) {
	using namespace std;
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::CREATE_CHARACTER);

	RakNet::BitStream data;

	LwoNameValue ldfData;
	ldfData.Insert<LWOOBJID>(u"objid", player);
	ldfData.Insert<LOT>(u"template", 1);
	ldfData.Insert<string>(u"xmlData", xmlData);
	ldfData.Insert<u16string>(u"name", username);
	ldfData.Insert<int32_t>(u"gmlevel", static_cast<int32_t>(gm));
	ldfData.Insert<int32_t>(u"chatmode", static_cast<int32_t>(gm));
	ldfData.Insert<int64_t>(u"reputation", reputation);
	ldfData.Insert<int32_t>(u"propertycloneid", cloneID);
	
	data.Write<uint32_t>(ldfData.values.size());
	for (const auto& toSerialize : ldfData | std::views::values) toSerialize->WriteToPacket(data);

	//Compress the data before sending:
	const uint32_t reservedSize = ZCompression::GetMaxCompressedLength(data.GetNumberOfBytesUsed());
	auto compressedData = std::make_unique<uint8_t[]>(reservedSize);

	size_t size = ZCompression::Compress(data.GetData(), data.GetNumberOfBytesUsed(), compressedData.get(), reservedSize);

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
	bitStream.WriteAlignedBytes(compressedData.get(), size);
#pragma warning(default:6385)

	SEND_PACKET;
	LOG("Sent CreateCharacter for ID: %llu", player);
}

inline void SendChatModerationResponse(const SystemAddress& sysAddr, bool requestAccepted, uint32_t requestID, const std::string& receiver, std::set<std::pair<uint8_t, uint8_t>> unacceptedItems) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::CHAT_MODERATION_STRING);

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

inline void SendGMLevelChange(const SystemAddress& sysAddr, bool success, eGameMasterLevel highestLevel, eGameMasterLevel prevLevel, eGameMasterLevel newLevel) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::MAKE_GM_RESPONSE);

	bitStream.Write<uint8_t>(success);
	bitStream.Write(static_cast<uint16_t>(highestLevel));
	bitStream.Write(static_cast<uint16_t>(prevLevel));
	bitStream.Write(static_cast<uint16_t>(newLevel));

	SEND_PACKET;
}

inline void SendHTTPMonitorInfo(const SystemAddress& sysAddr, const HTTPMonitorInfo& info) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::HTTP_MONITOR_INFO_RESPONSE);
	info.Serialize(bitStream);
	SEND_PACKET;
}

inline void SendDebugOuput(const SystemAddress& sysAddr, const std::string& data) {
	CBITSTREAM;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::DEBUG_OUTPUT);
	bitStream.Write<uint32_t>(data.size());
	bitStream.Write(data);
	SEND_PACKET;
}

struct ChatMessage {
	uint8_t chatChannel = 0;
	uint16_t unknown = 0;
	std::u16string message;
};

struct ChatModerationRequest {
	uint8_t chatLevel = 0;
	uint8_t requestID = 0;
	std::string receiver;
	std::string message;
};

inline ChatMessage HandleChatMessage(Packet* packet) {
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

inline PositionUpdate HandleClientPositionUpdate(Packet* packet) {
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

inline ChatModerationRequest HandleChatModerationRequest(Packet* packet) {
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

inline int32_t SendTop5HelpIssues(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	int32_t language = 0;
	inStream.Read(language);
	return language;
}

	// ---- UserManager::RequestCharacterList, the part that wrote the packet (characters[i] is any type with
	// Character's getters) ----
	template<typename CharacterT>
	inline void SendCharacterList(const SystemAddress& sysAddr, std::vector<CharacterT*> characters) {
	RakNet::BitStream bitStream;
	BitStreamUtils::WriteHeader(bitStream, ServiceType::CLIENT, MessageType::Client::CHARACTER_LIST_RESPONSE);

	bitStream.Write<uint8_t>(characters.size());
	bitStream.Write<uint8_t>(0); //TODO: Pick the most recent played index.  character index in front, just picking 0

	for (uint32_t i = 0; i < characters.size(); ++i) {
		bitStream.Write(characters[i]->GetObjectID());
		bitStream.Write<uint32_t>(0);

		bitStream.Write(LUWString(characters[i]->GetName()));
		bitStream.Write(LUWString(characters[i]->GetUnapprovedName()));

		bitStream.Write<uint8_t>(characters[i]->GetNameRejected());
		bitStream.Write<uint8_t>(false);

		bitStream.Write(LUString("", 10));

		bitStream.Write(characters[i]->GetShirtColor());
		bitStream.Write(characters[i]->GetShirtStyle());
		bitStream.Write(characters[i]->GetPantsColor());
		bitStream.Write(characters[i]->GetHairStyle());
		bitStream.Write(characters[i]->GetHairColor());
		bitStream.Write(characters[i]->GetLeftHand());
		bitStream.Write(characters[i]->GetRightHand());
		bitStream.Write(characters[i]->GetEyebrows());
		bitStream.Write(characters[i]->GetEyes());
		bitStream.Write(characters[i]->GetMouth());
		bitStream.Write<uint32_t>(0);

		bitStream.Write<uint16_t>(characters[i]->GetZoneID());
		bitStream.Write<uint16_t>(characters[i]->GetZoneInstance());
		bitStream.Write(characters[i]->GetZoneClone());

		bitStream.Write(characters[i]->GetLastLogin());

		const auto& equippedItems = characters[i]->GetEquippedItems();
		bitStream.Write<uint16_t>(equippedItems.size());

		for (uint32_t j = 0; j < equippedItems.size(); ++j) {
			bitStream.Write(equippedItems[j]);
		}
	}

	SEND_PACKET;
	}

	// ---- UserManager::CreateCharacter, the reads ----
	struct CreateCharacterRead {
	LUWString LUWStringName;
	uint32_t firstNameIndex;
	uint32_t middleNameIndex;
	uint32_t lastNameIndex;
	uint32_t shirtColor;
	uint32_t shirtStyle;
	uint32_t pantsColor;
	uint32_t hairStyle;
	uint32_t hairColor;
	uint32_t lh;
	uint32_t rh;
	uint32_t eyebrows;
	uint32_t eyes;
	uint32_t mouth;
	};

	inline CreateCharacterRead ReadCreateCharacter(Packet* packet) {
	CreateCharacterRead r{};
	auto& [LUWStringName, firstNameIndex, middleNameIndex, lastNameIndex, shirtColor, shirtStyle, pantsColor, hairStyle, hairColor, lh, rh, eyebrows, eyes, mouth] = r;
	CINSTREAM_SKIP_HEADER;
	inStream.Read(LUWStringName);
	inStream.Read(firstNameIndex);
	inStream.Read(middleNameIndex);
	inStream.Read(lastNameIndex);
	inStream.IgnoreBytes(9);
	inStream.Read(shirtColor);
	inStream.Read(shirtStyle);
	inStream.Read(pantsColor);
	inStream.Read(hairStyle);
	inStream.Read(hairColor);
	inStream.Read(lh);
	inStream.Read(rh);
	inStream.Read(eyebrows);
	inStream.Read(eyes);
	inStream.Read(mouth);
	return r;
	}

	// ---- UserManager::DeleteCharacter, the read ----
	inline LWOOBJID ReadDeleteCharacter(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID objectID;
	inStream.Read(objectID);
	return objectID;
	}

	// ---- UserManager::RenameCharacter, the reads ----
	inline std::pair<LWOOBJID, std::string> ReadRenameCharacter(Packet* packet) {
	CINSTREAM_SKIP_HEADER;
	LWOOBJID objectID;
	inStream.Read(objectID);

	LUWString LUWStringName;
	inStream.Read(LUWStringName);
	auto newName = LUWStringName.GetAsString();
	return { objectID, newName };
	}

	// ---- WorldServer HandlePacket VALIDATION, the reads ----
	struct ValidationRead {
		std::string username;
		std::string sessionKey;
		std::string checksum;
	};

	inline ValidationRead ReadValidation(Packet* packet) {
		CINSTREAM_SKIP_HEADER;
		LUWString username;
		inStream.Read(username);

		LUWString sessionKey;
		// sometimes client puts a null terminator at the end of the checksum and sometimes doesn't, weird
		inStream.Read(sessionKey);
		LUString clientDatabaseChecksum(32);
		inStream.Read(clientDatabaseChecksum);
		return { username.GetAsString(), sessionKey.GetAsString(), clientDatabaseChecksum.string };
	}

	// ---- WorldServer HandlePacket LOGIN_REQUEST, the read ----
	inline LWOOBJID ReadLoginRequest(Packet* packet) {
		CINSTREAM_SKIP_HEADER;

		LWOOBJID playerID = 0;
		inStream.Read(playerID);
		return playerID;
	}

	// ---- WorldServer HandlePacket GAME_MSG, the reads ----
	struct GameMessageRead {
		LWOOBJID objectID;
		MessageType::Game messageID;
		std::vector<uint8_t> data;
		uint32_t dataBits;
	};

	inline GameMessageRead ReadGameMessage(Packet* packet) {
		RakNet::BitStream bitStream(packet->data, packet->length, false);

		uint64_t header;
		LWOOBJID objectID;
		MessageType::Game messageID;

		bitStream.Read(header);
		bitStream.Read(objectID);
		bitStream.Read(messageID);

		RakNet::BitStream dataStream;
		bitStream.Read(dataStream, bitStream.GetNumberOfUnreadBits());
		return { objectID, messageID, { dataStream.GetData(), dataStream.GetData() + dataStream.GetNumberOfBytesUsed() }, static_cast<uint32_t>(dataStream.GetNumberOfBitsUsed()) };
	}

	// ---- WorldServer HandlePacket ROUTE_PACKET: the packet it built for the chat server (it then sent it with
	// Game::chatServer->Send); objectID is what it looked up for the sender. Returns false where it returned. ----
	inline bool BuildRoutePacket(Packet* packet, LWOOBJID objectID, RakNet::BitStream& bitStream) {
		//Yeet to chat
		CINSTREAM_SKIP_HEADER;
		uint32_t size = 0;
		inStream.Read(size);

		if (size > 20000) {
			LOG("Tried to route a packet with a read size > 20000, so likely a false packet.");
			return false;
		}

		BitStreamUtils::WriteHeader(bitStream, ServiceType::CHAT, packet->data[14]);

		bitStream.Write(objectID);

		//Now write the rest of the data:
		auto data = inStream.GetData();
		for (uint32_t i = 23; i - 23 < size && i < packet->length; ++i) {
			bitStream.Write(data[i]);
		}
		return true;
	}

	// ---- WorldServer HandlePacket HANDLE_FUNNESS, the reads ----
	inline CaughtFunness ReadFunness(Packet* packet) {
		CINSTREAM_SKIP_HEADER;
		CaughtFunness funness;
		inStream.Read(funness.cheatInfo);
		inStream.Read(funness.cheatType);
		return funness;
	}
}

#endif // WORLDPACKETSLEGACY_H
