#include "WorldPackets.h"

#include "dCommonVars.h"

#include <algorithm>

namespace {
	// Copies every unread bit of from into to.
	bool ReadRest(RakNet::BitStream& from, RakNet::BitStream& to) {
		to.Reset();
		return from.Read(to, from.GetNumberOfUnreadBits());
	}

	void WriteAll(RakNet::BitStream& to, const RakNet::BitStream& from) {
		auto& nonConst = const_cast<RakNet::BitStream&>(from); // GetData is not const in RakNet
		to.WriteBits(nonConst.GetData(), nonConst.GetNumberOfBitsUsed(), false);
	}
}

namespace WorldPackets {
	void Validation::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(username);
		bitStream.Write(sessionKey);
		bitStream.Write(fdbChecksum);
	}

	bool Validation::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(username));
		// sometimes client puts a null terminator at the end of the checksum and sometimes doesn't, weird
		VALIDATE_READ(bitStream.Read(sessionKey));
		VALIDATE_READ(bitStream.Read(fdbChecksum));
		return true;
	}

	void CharacterCreateRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(name);
		bitStream.Write(firstNameIndex);
		bitStream.Write(middleNameIndex);
		bitStream.Write(lastNameIndex);
		for (const auto byte : unknown) bitStream.Write(byte);
		bitStream.Write(shirtColor);
		bitStream.Write(shirtStyle);
		bitStream.Write(pantsColor);
		bitStream.Write(hairStyle);
		bitStream.Write(hairColor);
		bitStream.Write(leftHand);
		bitStream.Write(rightHand);
		bitStream.Write(eyebrows);
		bitStream.Write(eyes);
		bitStream.Write(mouth);
	}

	bool CharacterCreateRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(name));
		VALIDATE_READ(bitStream.Read(firstNameIndex));
		VALIDATE_READ(bitStream.Read(middleNameIndex));
		VALIDATE_READ(bitStream.Read(lastNameIndex));
		for (auto& byte : unknown) VALIDATE_READ(bitStream.Read(byte));
		VALIDATE_READ(bitStream.Read(shirtColor));
		VALIDATE_READ(bitStream.Read(shirtStyle));
		VALIDATE_READ(bitStream.Read(pantsColor));
		VALIDATE_READ(bitStream.Read(hairStyle));
		VALIDATE_READ(bitStream.Read(hairColor));
		VALIDATE_READ(bitStream.Read(leftHand));
		VALIDATE_READ(bitStream.Read(rightHand));
		VALIDATE_READ(bitStream.Read(eyebrows));
		VALIDATE_READ(bitStream.Read(eyes));
		VALIDATE_READ(bitStream.Read(mouth));
		return true;
	}

	void CharacterLoginRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(playerID);
	}

	bool CharacterLoginRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(playerID));
		return true;
	}

	void GameMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objectID);
		bitStream.Write(messageID);
		WriteAll(bitStream, data);
	}

	bool GameMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objectID));
		VALIDATE_READ(bitStream.Read(messageID));
		VALIDATE_READ(ReadRest(bitStream, data));
		return true;
	}

	void CharacterDeleteRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objectID);
	}

	bool CharacterDeleteRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objectID));
		return true;
	}

	void CharacterRenameRequest::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(objectID);
		bitStream.Write(name);
	}

	bool CharacterRenameRequest::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(objectID));
		VALIDATE_READ(bitStream.Read(name));
		return true;
	}

	void LevelLoadComplete::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(mapID);
		bitStream.Write(instanceID);
		bitStream.Write(cloneID);
	}

	bool LevelLoadComplete::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(mapID));
		VALIDATE_READ(bitStream.Read(instanceID));
		VALIDATE_READ(bitStream.Read(cloneID));
		return true;
	}

	void PositionUpdate::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(update.position.x);
		bitStream.Write(update.position.y);
		bitStream.Write(update.position.z);

		bitStream.Write(update.rotation.x);
		bitStream.Write(update.rotation.y);
		bitStream.Write(update.rotation.z);
		bitStream.Write(update.rotation.w);

		bitStream.Write(update.onGround);
		bitStream.Write(update.onRail);

		bitStream.Write(hasVelocity);
		if (hasVelocity) {
			bitStream.Write(update.velocity.x);
			bitStream.Write(update.velocity.y);
			bitStream.Write(update.velocity.z);
		}

		bitStream.Write(hasAngularVelocity);
		if (hasAngularVelocity) {
			bitStream.Write(update.angularVelocity.x);
			bitStream.Write(update.angularVelocity.y);
			bitStream.Write(update.angularVelocity.z);
		}

		bitStream.Write(hasLocalSpaceInfo);
		if (hasLocalSpaceInfo) {
			bitStream.Write(update.localSpaceInfo.objectId);
			bitStream.Write(update.localSpaceInfo.position.x);
			bitStream.Write(update.localSpaceInfo.position.y);
			bitStream.Write(update.localSpaceInfo.position.z);
			bitStream.Write(hasLinearVelocity);
			if (hasLinearVelocity) {
				bitStream.Write(update.localSpaceInfo.linearVelocity.x);
				bitStream.Write(update.localSpaceInfo.linearVelocity.y);
				bitStream.Write(update.localSpaceInfo.linearVelocity.z);
			}
		}

		bitStream.Write(hasRemoteInputInfo);
		if (hasRemoteInputInfo) {
			bitStream.Write(update.remoteInputInfo.m_RemoteInputX);
			bitStream.Write(update.remoteInputInfo.m_RemoteInputY);
			bitStream.Write(update.remoteInputInfo.m_IsPowersliding);
			bitStream.Write(update.remoteInputInfo.m_IsModified);
		}
	}

	bool PositionUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(update.position.x));
		VALIDATE_READ(bitStream.Read(update.position.y));
		VALIDATE_READ(bitStream.Read(update.position.z));

		VALIDATE_READ(bitStream.Read(update.rotation.x));
		VALIDATE_READ(bitStream.Read(update.rotation.y));
		VALIDATE_READ(bitStream.Read(update.rotation.z));
		VALIDATE_READ(bitStream.Read(update.rotation.w));

		VALIDATE_READ(bitStream.Read(update.onGround));
		VALIDATE_READ(bitStream.Read(update.onRail));

		VALIDATE_READ(bitStream.Read(hasVelocity));
		if (hasVelocity) {
			VALIDATE_READ(bitStream.Read(update.velocity.x));
			VALIDATE_READ(bitStream.Read(update.velocity.y));
			VALIDATE_READ(bitStream.Read(update.velocity.z));
		}

		VALIDATE_READ(bitStream.Read(hasAngularVelocity));
		if (hasAngularVelocity) {
			VALIDATE_READ(bitStream.Read(update.angularVelocity.x));
			VALIDATE_READ(bitStream.Read(update.angularVelocity.y));
			VALIDATE_READ(bitStream.Read(update.angularVelocity.z));
		}

		// The last two sections may be missing entirely (DLU has always accepted that)
		hasLocalSpaceInfo = false;
		hasLinearVelocity = false;
		if (bitStream.Read(hasLocalSpaceInfo) && hasLocalSpaceInfo) {
			VALIDATE_READ(bitStream.Read(update.localSpaceInfo.objectId));
			VALIDATE_READ(bitStream.Read(update.localSpaceInfo.position.x));
			VALIDATE_READ(bitStream.Read(update.localSpaceInfo.position.y));
			VALIDATE_READ(bitStream.Read(update.localSpaceInfo.position.z));
			if (bitStream.Read(hasLinearVelocity) && hasLinearVelocity) {
				VALIDATE_READ(bitStream.Read(update.localSpaceInfo.linearVelocity.x));
				VALIDATE_READ(bitStream.Read(update.localSpaceInfo.linearVelocity.y));
				VALIDATE_READ(bitStream.Read(update.localSpaceInfo.linearVelocity.z));
			}
		}

		hasRemoteInputInfo = false;
		if (bitStream.Read(hasRemoteInputInfo) && hasRemoteInputInfo) {
			VALIDATE_READ(bitStream.Read(update.remoteInputInfo.m_RemoteInputX));
			VALIDATE_READ(bitStream.Read(update.remoteInputInfo.m_RemoteInputY));
			VALIDATE_READ(bitStream.Read(update.remoteInputInfo.m_IsPowersliding));
			VALIDATE_READ(bitStream.Read(update.remoteInputInfo.m_IsModified));
		}

		return true;
	}

	void TmpGuildCreate::Serialize(RakNet::BitStream& bitStream) const {
		auto name = guildName;
		if (name.size() >= NAME_SIZE) name.resize(NAME_SIZE - 1);
		bitStream.Write(LUWString(name, NAME_SIZE));
	}

	bool TmpGuildCreate::Deserialize(RakNet::BitStream& bitStream) {
		LUWString name(NAME_SIZE);
		VALIDATE_READ(bitStream.Read(name));
		guildName = name.string;
		return true;
	}

	void MailPacket::Serialize(RakNet::BitStream& bitStream) const {
		WriteAll(bitStream, data);
	}

	bool MailPacket::Deserialize(RakNet::BitStream& bitStream) {
		return ReadRest(bitStream, data);
	}

	void RoutePacket::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(size);
		bitStream.Write(routedService);
		bitStream.Write(routedMessageID);
		bitStream.Write(padding);
		for (const auto byte : routedData) bitStream.Write(byte);
	}

	bool RoutePacket::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(size));
		VALIDATE_READ(bitStream.Read(routedService));
		VALIDATE_READ(bitStream.Read(routedMessageID));
		VALIDATE_READ(bitStream.Read(padding));
		routedData.resize(BITS_TO_BYTES(bitStream.GetNumberOfUnreadBits()));
		if (!routedData.empty()) VALIDATE_READ(bitStream.ReadBits(routedData.data(), BYTES_TO_BITS(routedData.size()), true));
		return true;
	}

	ChatPackets::RoutedFromClient RoutePacket::ToChat(const LWOOBJID senderID) const {
		constexpr size_t ROUTED_DATA_SKIP = 4;
		ChatPackets::RoutedFromClient routed(static_cast<uint8_t>(routedMessageID & 0xFF));
		routed.senderID = senderID;
		for (size_t i = ROUTED_DATA_SKIP; i - ROUTED_DATA_SKIP < size && i < routedData.size(); ++i) {
			routed.data.push_back(routedData[i]);
		}
		return routed;
	}

	void StringCheck::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(chatLevel);
		bitStream.Write(requestID);
		auto fixedReceiver = receiver;
		fixedReceiver.resize(42);
		bitStream.Write(fixedReceiver);
		bitStream.Write<uint16_t>(message.size());
		bitStream.Write(message);
	}

	bool StringCheck::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(chatLevel));
		VALIDATE_READ(bitStream.Read(requestID));
		receiver.resize(42);
		VALIDATE_READ(bitStream.ReadBits(reinterpret_cast<unsigned char*>(receiver.data()), BYTES_TO_BITS(receiver.size() * sizeof(char16_t)), true));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint16_t>(bitStream, message));
		return true;
	}

	std::string StringCheck::GetNarrowReceiver() const {
		std::string narrow;
		for (const auto character : receiver) narrow.push_back(static_cast<uint8_t>(character));

		if (!narrow.empty()) {
			if (std::string(narrow.c_str(), 4) == "[GM]") { // Shift the string forward if we are speaking to a GM as the client appends "[GM]" if they are
				narrow = std::string(narrow.c_str() + 4, narrow.size() - 4);
			}
		}
		return narrow;
	}

	std::string StringCheck::GetNarrowMessage() const {
		std::string narrow;
		for (const auto character : message) narrow.push_back(static_cast<uint8_t>(character));
		return narrow;
	}

	void GeneralChatMessage::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(chatChannel);
		bitStream.Write(unknown);
		bitStream.Write<int32_t>(message.size() + 1);
		bitStream.Write(message);
		bitStream.Write<char16_t>(u'\0');
	}

	bool GeneralChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		int32_t messageLength{};

		VALIDATE_READ(bitStream.Read(chatChannel));
		VALIDATE_READ(bitStream.Read(unknown));
		VALIDATE_READ(bitStream.Read(messageLength));

		if (messageLength < 0 || static_cast<uint32_t>(messageLength) > MAX_MESSAGE_LENGTH) return false;

		message.clear();
		for (int32_t i = 0; i < (messageLength - 1); ++i) {
			char16_t character;
			VALIDATE_READ(bitStream.Read(character));
			message.push_back(character);
		}

		// The null terminator the count includes; DLU never needed it
		if (messageLength > 0) bitStream.IgnoreBits(std::min<BitSize_t>(bitStream.GetNumberOfUnreadBits(), BYTES_TO_BITS(sizeof(char16_t))));
		return true;
	}

	void HandleFunness::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(cheatInfo);
		bitStream.Write(cheatType);
	}

	bool HandleFunness::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(cheatInfo));
		VALIDATE_READ(bitStream.Read(cheatType));
		return true;
	}

	void RequestUgcManifestInfo::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(blueprintId);
		bitStream.Write(resourceType);
	}

	bool RequestUgcManifestInfo::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(blueprintId));
		VALIDATE_READ(bitStream.Read(resourceType));
		return true;
	}

	void UgcDownloadFailed::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(resType);
		bitStream.Write(blueprintId);
		bitStream.Write(statusCode);
		bitStream.Write(charId);
	}

	bool UgcDownloadFailed::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(resType));
		VALIDATE_READ(bitStream.Read(blueprintId));
		VALIDATE_READ(bitStream.Read(statusCode));
		VALIDATE_READ(bitStream.Read(charId));
		return true;
	}

	void UIHelpTop5::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(language);
	}

	bool UIHelpTop5::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(language));
		return true;
	}
}
