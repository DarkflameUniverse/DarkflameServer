#ifndef WORLDPACKETS_H
#define WORLDPACKETS_H

#include "dCommonVars.h"
#include "BitStreamUtils.h"
#include "MessageType/World.h"
#include "eChatMode.h"

class Entity;
enum class eLanguageCodeID : int32_t {
	EN_US = 0,
	PL_US = 1,
	DE_DE = 2,
	EN_GB = 3
};

namespace WorldPackets {

	struct UIHelpTop5: public LUBitStream {
		eLanguageCodeID languageCode = eLanguageCodeID::EN_US;

		UIHelpTop5() : LUBitStream(eConnectionType::WORLD, MessageType::World::UI_HELP_TOP_5) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct GeneralChatMessage : public LUBitStream {
		uint8_t chatChannel = 0;
		std::u16string message;

		GeneralChatMessage() : LUBitStream(eConnectionType::WORLD, MessageType::World::GENERAL_CHAT_MESSAGE) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct PositionUpdate : public LUBitStream {
		NiPoint3 position = NiPoint3Constant::ZERO;
		NiQuaternion rotation = NiQuaternionConstant::IDENTITY;
		bool onGround = false;
		bool onRail = false;
		NiPoint3 velocity = NiPoint3Constant::ZERO;
		NiPoint3 angularVelocity = NiPoint3Constant::ZERO;
		struct LocalSpaceInfo {
			LWOOBJID objectId = LWOOBJID_EMPTY;
			NiPoint3 position = NiPoint3Constant::ZERO;
			NiPoint3 linearVelocity = NiPoint3Constant::ZERO;
		};
		LocalSpaceInfo localSpaceInfo;
		struct RemoteInputInfo {
			bool operator==(const RemoteInputInfo& other) {
				return m_RemoteInputX == other.m_RemoteInputX && m_RemoteInputY == other.m_RemoteInputY && m_IsPowersliding == other.m_IsPowersliding && m_IsModified == other.m_IsModified;
			}
			float m_RemoteInputX = 0;
			float m_RemoteInputY = 0;
			bool m_IsPowersliding = false;
			bool m_IsModified = false;
		};
		RemoteInputInfo remoteInputInfo;

		PositionUpdate() : LUBitStream(eConnectionType::WORLD, MessageType::World::POSITION_UPDATE) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct StringCheck : public LUBitStream {
		eChatMode chatMode = eChatMode::RESTRICTED;
		uint8_t requestID = 0;
		std::string receiver;
		std::string message;

		StringCheck() : LUBitStream(eConnectionType::WORLD, MessageType::World::STRING_CHECK) {};
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};
}

#endif // WORLDPACKETS_H
