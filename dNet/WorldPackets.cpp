#include "WorldPackets.h"
#include "Amf3.h"
#include "dConfig.h"
#include "GameMessages.h"
#include "Entity.h"

namespace WorldPackets {
	
	bool UIHelpTop5::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(languageCode));
		return true;
	}

	void UIHelpTop5::Handle() {
		AMFArrayValue data;
		switch (languageCode) {
			case eLanguageCodeID::EN_US:
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
				break;
			case eLanguageCodeID::PL_US:
			[[fallthrough]];
			case eLanguageCodeID::DE_DE:
			[[fallthrough]];
			case eLanguageCodeID::EN_GB:
			[[fallthrough]];
			default:
				break;
		}
		GameMessages::SendUIMessageServerToSingleClient(player, sysAddr, "UIHelpTop5", data);
	}

	bool GeneralChatMessage::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(chatChannel));
		VALIDATE_READ(bitStream.Read(unknown));

		uint32_t messageLength;
		VALIDATE_READ(bitStream.Read(messageLength));

		for (uint32_t i = 0; i < (messageLength - 1); ++i) {
			uint16_t character;
			VALIDATE_READ(bitStream.Read(character));
			message.push_back(character);
		}

		return true;
	}

	void GeneralChatMessage::Handle() {
	}

	bool PositionUpdate::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(position.x));
		VALIDATE_READ(bitStream.Read(position.y));
		VALIDATE_READ(bitStream.Read(position.z));

		VALIDATE_READ(bitStream.Read(rotation.x));
		VALIDATE_READ(bitStream.Read(rotation.y));
		VALIDATE_READ(bitStream.Read(rotation.z));
		VALIDATE_READ(bitStream.Read(rotation.w));

		VALIDATE_READ(bitStream.Read(onGround));
		VALIDATE_READ(bitStream.Read(onRail));

		bool velocityFlag = false;
		if (bitStream.Read(velocityFlag) && velocityFlag) {
			VALIDATE_READ(bitStream.Read(velocity.x));
			VALIDATE_READ(bitStream.Read(velocity.y));
			VALIDATE_READ(bitStream.Read(velocity.z));
		}

		bool angVelocityFlag = false;
		if (bitStream.Read(angVelocityFlag) && angVelocityFlag) {
			VALIDATE_READ(bitStream.Read(angularVelocity.x));
			VALIDATE_READ(bitStream.Read(angularVelocity.y));
			VALIDATE_READ(bitStream.Read(angularVelocity.z));
		}

		// TODO figure out how to use these. Ignoring for now, but reading in if they exist.
		bool hasLocalSpaceInfo{};
		VALIDATE_READ(bitStream.Read(hasLocalSpaceInfo));
		if (hasLocalSpaceInfo) {
			VALIDATE_READ(bitStream.Read(localSpaceInfo.objectId));
			VALIDATE_READ(bitStream.Read(localSpaceInfo.position.x));
			VALIDATE_READ(bitStream.Read(localSpaceInfo.position.y));
			VALIDATE_READ(bitStream.Read(localSpaceInfo.position.z));

			bool hasLinearVelocity = false;
			if (bitStream.Read(hasLinearVelocity) && hasLinearVelocity) {
				VALIDATE_READ(bitStream.Read(localSpaceInfo.linearVelocity.x));
				VALIDATE_READ(bitStream.Read(localSpaceInfo.linearVelocity.y));
				VALIDATE_READ(bitStream.Read(localSpaceInfo.linearVelocity.z));
			}
		}
		bool hasRemoteInputInfo{};
		VALIDATE_READ(bitStream.Read(hasRemoteInputInfo));
		if (hasRemoteInputInfo) {
			VALIDATE_READ(bitStream.Read(remoteInputInfo.m_RemoteInputX));
			VALIDATE_READ(bitStream.Read(remoteInputInfo.m_RemoteInputY));
			VALIDATE_READ(bitStream.Read(remoteInputInfo.m_IsPowersliding));
			VALIDATE_READ(bitStream.Read(remoteInputInfo.m_IsModified));
		}

		return true;
	}

	void PositionUpdate::Handle() {
		
	}

	bool StringCheck::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(chatLevel));
		VALIDATE_READ(bitStream.Read(requestID));

		for (uint32_t i = 0; i < 42; ++i) {
			uint16_t character;
			VALIDATE_READ(bitStream.Read(character));
			receiver.push_back(static_cast<uint8_t>(character));
		}

		if (!receiver.empty()) {
			if (std::string(receiver.c_str(), 4) == "[GM]") { // Shift the string forward if we are speaking to a GM as the client appends "[GM]" if they are
				receiver = std::string(receiver.c_str() + 4, receiver.size() - 4);
			}
		}

		uint32_t messageLength;
		VALIDATE_READ(bitStream.Read(messageLength));
		for (uint32_t i = 0; i < messageLength; ++i) {
			uint16_t character;
			VALIDATE_READ(bitStream.Read(character));
			message.push_back(static_cast<uint8_t>(character));
		}

		return true;
	}

	void StringCheck::Handle() {
	}

}
