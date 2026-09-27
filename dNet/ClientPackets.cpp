/*
 * Darkflame Universe
 * Copyright 2018
 */

#include "ClientPackets.h"
#include "dCommonVars.h"
#include "eCharacterCreationResponse.h"
#include "eGameMasterLevel.h"
#include "eRenameResponse.h"
#include "LDFFormat.h"
#include "ZCompression.h"

#include <cassert>
#include <memory>
#include <ranges>
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

	void LoadStaticZone::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(mapID);
		bitStream.Write(instanceID);
		bitStream.Write(cloneID);
		bitStream.Write(mapChecksum);
		bitStream.Write(editorEnabled);
		bitStream.Write(editorLevel);
		bitStream.Write(playerPosition.x);
		bitStream.Write(playerPosition.y);
		bitStream.Write(playerPosition.z);
		bitStream.Write(instanceType);
	}

	bool LoadStaticZone::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(mapID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(cloneID));
		VALIDATE_READ(bitStream.Read(mapChecksum));
		VALIDATE_READ(bitStream.Read(editorEnabled));
		VALIDATE_READ(bitStream.Read(editorLevel));
		VALIDATE_READ(bitStream.Read(playerPosition.x));
		VALIDATE_READ(bitStream.Read(playerPosition.y));
		VALIDATE_READ(bitStream.Read(playerPosition.z));
		VALIDATE_READ(bitStream.Read(instanceType));
		return true;
	}

	void CharacterListResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint8_t>(characters.size());
		bitStream.Write(selectedCharacterIndex);

		for (const auto& character : characters) {
			bitStream.Write(character.objectID);
			bitStream.Write(character.unknown1);

			bitStream.Write(character.name);
			bitStream.Write(character.unapprovedName);

			bitStream.Write<uint8_t>(character.nameRejected);
			bitStream.Write<uint8_t>(character.isFreeToPlay);

			bitStream.Write(character.unknown2);

			bitStream.Write(character.shirtColor);
			bitStream.Write(character.shirtStyle);
			bitStream.Write(character.pantsColor);
			bitStream.Write(character.hairStyle);
			bitStream.Write(character.hairColor);
			bitStream.Write(character.leftHand);
			bitStream.Write(character.rightHand);
			bitStream.Write(character.eyebrows);
			bitStream.Write(character.eyes);
			bitStream.Write(character.mouth);
			bitStream.Write(character.unknown3);

			bitStream.Write(character.zoneID);
			bitStream.Write(character.zoneInstance);
			bitStream.Write(character.zoneClone);

			bitStream.Write(character.lastLogin);

			bitStream.Write<uint16_t>(character.equippedItems.size());
			for (const auto item : character.equippedItems) bitStream.Write(item);
		}
	}

	bool CharacterListResponse::Deserialize(RakNet::BitStream& bitStream) {
		uint8_t count{};
		VALIDATE_READ(bitStream.Read(count));
		VALIDATE_READ(bitStream.Read(selectedCharacterIndex));

		characters.resize(count);
		for (auto& character : characters) {
			VALIDATE_READ(bitStream.Read(character.objectID));
			VALIDATE_READ(bitStream.Read(character.unknown1));

			VALIDATE_READ(bitStream.Read(character.name));
			VALIDATE_READ(bitStream.Read(character.unapprovedName));

			uint8_t flag{};
			VALIDATE_READ(bitStream.Read(flag));
			character.nameRejected = flag != 0;
			VALIDATE_READ(bitStream.Read(flag));
			character.isFreeToPlay = flag != 0;

			VALIDATE_READ(bitStream.Read(character.unknown2));

			VALIDATE_READ(bitStream.Read(character.shirtColor));
			VALIDATE_READ(bitStream.Read(character.shirtStyle));
			VALIDATE_READ(bitStream.Read(character.pantsColor));
			VALIDATE_READ(bitStream.Read(character.hairStyle));
			VALIDATE_READ(bitStream.Read(character.hairColor));
			VALIDATE_READ(bitStream.Read(character.leftHand));
			VALIDATE_READ(bitStream.Read(character.rightHand));
			VALIDATE_READ(bitStream.Read(character.eyebrows));
			VALIDATE_READ(bitStream.Read(character.eyes));
			VALIDATE_READ(bitStream.Read(character.mouth));
			VALIDATE_READ(bitStream.Read(character.unknown3));

			VALIDATE_READ(bitStream.Read(character.zoneID));
			VALIDATE_READ(bitStream.Read(character.zoneInstance));
			VALIDATE_READ(bitStream.Read(character.zoneClone));

			VALIDATE_READ(bitStream.Read(character.lastLogin));

			uint16_t itemCount{};
			VALIDATE_READ(bitStream.Read(itemCount));
			if (itemCount > BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()) / sizeof(LOT)) return false;
			character.equippedItems.resize(itemCount);
			for (auto& item : character.equippedItems) VALIDATE_READ(bitStream.Read(item));
		}
		return true;
	}

	void CharacterCreateResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(response);
	}

	bool CharacterCreateResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(response));
		return true;
	}

	void CharacterRenameResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(response);
	}

	bool CharacterRenameResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(response));
		return true;
	}

	void DeleteCharacterResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint8_t>(success);
	}

	bool DeleteCharacterResponse::Deserialize(RakNet::BitStream& bitStream) {
		uint8_t flag{};
		VALIDATE_READ(bitStream.Read(flag));
		success = flag != 0;
		return true;
	}

	void TransferToWorld::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(serverIP);
		bitStream.Write(serverPort);
		bitStream.Write<uint8_t>(mythranShift);
	}

	bool TransferToWorld::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(serverIP));
		VALIDATE_READ(bitStream.Read(serverPort));
		uint8_t flag{};
		VALIDATE_READ(bitStream.Read(flag));
		mythranShift = flag != 0;
		return true;
	}

	void ServerStates::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(serverState);
	}

	bool ServerStates::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(serverState));
		return true;
	}

	void CreateCharacter::Serialize(RakNet::BitStream& bitStream) const {
		using namespace std;
		RakNet::BitStream data;

		LwoNameValue ldfData;
		ldfData.Insert<LWOOBJID>(u"objid", objectID);
		ldfData.Insert<LOT>(u"template", templateID);
		ldfData.Insert<string>(u"xmlData", xmlData);
		ldfData.Insert<u16string>(u"name", name);
		ldfData.Insert<int32_t>(u"gmlevel", static_cast<int32_t>(gmLevel));
		ldfData.Insert<int32_t>(u"chatmode", chatMode);
		ldfData.Insert<int64_t>(u"reputation", reputation);
		ldfData.Insert<int32_t>(u"propertycloneid", propertyCloneID);

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
	}

	namespace {
		bool ReadLdfKey(RakNet::BitStream& bitStream, std::u16string& key) {
			uint8_t keyBytes{};
			VALIDATE_READ(bitStream.Read(keyBytes));
			key.resize(keyBytes / sizeof(char16_t));
			for (auto& character : key) VALIDATE_READ(bitStream.Read(character));
			return true;
		}
	}

	bool CreateCharacter::Deserialize(RakNet::BitStream& bitStream) {
		uint32_t sizePlusHeader{};
		uint8_t compressed{};
		uint32_t uncompressedSize{};
		uint32_t compressedSize{};
		VALIDATE_READ(bitStream.Read(sizePlusHeader));
		VALIDATE_READ(bitStream.Read(compressed));
		VALIDATE_READ(bitStream.Read(uncompressedSize));
		VALIDATE_READ(bitStream.Read(compressedSize));
		if (compressed != 1 || sizePlusHeader != compressedSize + 9) return false;
		if (compressedSize > BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()) || uncompressedSize > MAX_MESSAGE_LENGTH) return false;

		std::vector<uint8_t> compressedData(compressedSize);
		if (compressedSize > 0) VALIDATE_READ(bitStream.ReadAlignedBytes(compressedData.data(), compressedSize));
		std::vector<uint8_t> uncompressed(uncompressedSize);
		int32_t error{};
		const auto decompressedSize = ZCompression::Decompress(compressedData.data(), compressedSize, uncompressed.data(), uncompressedSize, error);
		if (decompressedSize != static_cast<int32_t>(uncompressedSize)) return false;

		RakNet::BitStream data(uncompressed.data(), uncompressed.size(), false);
		uint32_t count{};
		VALIDATE_READ(data.Read(count));
		for (uint32_t i = 0; i < count; i++) {
			std::u16string key;
			VALIDATE_READ(ReadLdfKey(data, key));
			uint8_t type{};
			VALIDATE_READ(data.Read(type));
			switch (type) {
			case LDF_TYPE_S32: {
				int32_t value{};
				VALIDATE_READ(data.Read(value));
				if (key == u"template") templateID = value;
				else if (key == u"gmlevel") gmLevel = static_cast<eGameMasterLevel>(value);
				else if (key == u"chatmode") chatMode = value;
				else if (key == u"propertycloneid") propertyCloneID = value;
				break;
			}
			case LDF_TYPE_OBJID: {
				int64_t value{};
				VALIDATE_READ(data.Read(value));
				if (key == u"objid") objectID = value;
				else if (key == u"reputation") reputation = value;
				break;
			}
			case LDF_TYPE_UTF_16: {
				std::u16string value;
				VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(data, value));
				if (key == u"name") name = value;
				break;
			}
			case LDF_TYPE_UTF_8: {
				std::string value;
				VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(data, value));
				if (key == u"xmlData") xmlData = value;
				break;
			}
			default:
				return false; // CreateCharacter never uses other types
			}
		}
		return true;
	}

	void ChatModerationString::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint8_t>(requestAccepted); // Is sentence ok?
		bitStream.Write(sourceID); // Source ID, unknown

		bitStream.Write(requestID); // request ID
		bitStream.Write(chatMode); // chat mode

		bitStream.Write(receiver); // receiver name

		for (const auto& [start, length] : rejectedSegments) {
			bitStream.Write(start); // start index
			bitStream.Write(length); // length
		}

		for (auto i = rejectedSegments.size(); i < 64; i++) {
			bitStream.Write<uint16_t>(0);
		}
	}

	bool ChatModerationString::Deserialize(RakNet::BitStream& bitStream) {
		uint8_t accepted{};
		VALIDATE_READ(bitStream.Read(accepted));
		requestAccepted = accepted != 0;
		VALIDATE_READ(bitStream.Read(sourceID));
		VALIDATE_READ(bitStream.Read(requestID));
		VALIDATE_READ(bitStream.Read(chatMode));
		VALIDATE_READ(bitStream.Read(receiver));

		rejectedSegments.clear();
		for (int i = 0; i < 64; i++) {
			uint8_t start{};
			uint8_t length{};
			VALIDATE_READ(bitStream.Read(start));
			VALIDATE_READ(bitStream.Read(length));
			if (start != 0 || length != 0) rejectedSegments.emplace(start, length);
		}
		return true;
	}

	void MakeGMResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write<uint8_t>(success);
		bitStream.Write(static_cast<uint16_t>(highestLevel));
		bitStream.Write(static_cast<uint16_t>(previousLevel));
		bitStream.Write(static_cast<uint16_t>(newLevel));
	}

	bool MakeGMResponse::Deserialize(RakNet::BitStream& bitStream) {
		uint8_t flag{};
		uint16_t highest{};
		uint16_t previous{};
		uint16_t next{};
		VALIDATE_READ(bitStream.Read(flag));
		VALIDATE_READ(bitStream.Read(highest));
		VALIDATE_READ(bitStream.Read(previous));
		VALIDATE_READ(bitStream.Read(next));
		success = flag != 0;
		highestLevel = static_cast<eGameMasterLevel>(highest);
		previousLevel = static_cast<eGameMasterLevel>(previous);
		newLevel = static_cast<eGameMasterLevel>(next);
		return true;
	}

	void HTTPMonitorInfoResponse::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(port);
		bitStream.Write<uint8_t>(openWeb);
		bitStream.Write<uint8_t>(supportsSum);
		bitStream.Write<uint8_t>(supportsDetail);
		bitStream.Write<uint8_t>(supportsWho);
		bitStream.Write<uint8_t>(supportsObjects);
	}

	bool HTTPMonitorInfoResponse::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(port));
		for (auto* flag : { &openWeb, &supportsSum, &supportsDetail, &supportsWho, &supportsObjects }) {
			uint8_t value{};
			VALIDATE_READ(bitStream.Read(value));
			*flag = value != 0;
		}
		return true;
	}

	void DebugOutput::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, data);
	}

	bool DebugOutput::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, data));
		return true;
	}
}
