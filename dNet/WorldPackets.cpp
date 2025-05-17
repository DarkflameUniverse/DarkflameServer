#include "WorldPackets.h"
#include "Amf3.h"
#include "dConfig.h"
#include "GameMessages.h"
#include "Entity.h"
#include "EntityManager.h"
#include "UserManager.h"
#include "User.h"
#include "Character.h"
#include "dChatFilter.h"

namespace WorldPackets {
	
	bool UIHelpTop5::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(languageCode));
		return true;
	}

	void UIHelpTop5::Handle() {
		Entity* player = Game::entityManager->GetEntity(objectID);
		if (!player) {
			LOG("Unable to get player for UIHelpTop5");
			return;
		}

		auto sysAddr = player->GetSystemAddress();
		if (sysAddr == UNASSIGNED_SYSTEM_ADDRESS) {
			LOG("Unable to get system address for player for UIHelpTop5");
			return;
		}
		
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
		uint16_t padding;
		VALIDATE_READ(bitStream.Read(padding));

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
		Entity* player = Game::entityManager->GetEntity(objectID);
		if (!player) return;
		auto sysAddr = player->GetSystemAddress();
		User* user = UserManager::Instance()->GetUser(sysAddr);
		if (!user) {
			LOG("Unable to get user to parse chat message");
			return;
		}

		std::string playerName = user->GetLastUsedChar()->GetName();
		bool isMythran = user->GetLastUsedChar()->GetGMLevel() > eGameMasterLevel::CIVILIAN;
		bool isOk = Game::chatFilter->IsSentenceOkay(GeneralUtils::UTF16ToWTF8(message), user->GetLastUsedChar()->GetGMLevel()).empty();
		LOG_DEBUG("Msg: %s was approved previously? %i", GeneralUtils::UTF16ToWTF8(message).c_str(), user->GetLastChatMessageApproved());
		if (!isOk) return;
		if (!isOk && !isMythran) return;

		std::string sMessage = GeneralUtils::UTF16ToWTF8(message);
		LOG("%s: %s", playerName.c_str(), sMessage.c_str());
		ChatPackets::SendChatMessage(sysAddr, chatChannel, playerName, user->GetLoggedInChar(), isMythran, message);
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
		Entity* entity = Game::entityManager->GetEntity(objectID);
		if (entity) entity->ProcessPositionUpdate(positionUpdate);

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
