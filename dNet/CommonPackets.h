#include "dCommonVars.h"
#include "dNetCommon.h"
#include "BitStreamUtils.h"
#include "MessageType/Server.h"

enum class eServerDisconnectIdentifiers : uint32_t {
	UNKNOWN_SERVER_ERROR = 0,
	WRONG_GAME_VERSION,
	WRONG_SERVER_VERSION,
	CONNECTION_ON_INVALID_PORT,
	DUPLICATE_LOGIN,
	SERVER_SHUTDOWN,
	SERVER_MAP_LOAD_FAILURE,
	INVALID_SESSION_KEY,
	ACCOUNT_NOT_IN_PENDING_LIST,
	CHARACTER_NOT_FOUND,
	CHARACTER_CORRUPTED,
	KICK,
	SAVE_FAILURE,
	FREE_TRIAL_EXPIRED,
	PLAY_SCHEDULE_TIME_DONE
};

// Packet Struct Functions
namespace CommonPackets {
	// Structs
	struct CommonLUBitStream : public LUBitStream {
		MessageType::Server messageType = MessageType::Server::VERSION_CONFIRM;

		CommonLUBitStream() : LUBitStream(ServiceType::COMMON) {};
		CommonLUBitStream(MessageType::Server _messageType) : LUBitStream(ServiceType::COMMON), messageType{_messageType} {};

		virtual void Serialize(RakNet::BitStream& bitStream) const override;
		virtual bool Deserialize(RakNet::BitStream& bitStream) override;
		virtual void Handle() override {};
	};

	struct VersionConfirm : public CommonLUBitStream {
		uint32_t netVersion = 0;
		ServiceType serviceType;
		uint32_t processID = 0;
		uint16_t port = 0;

		VersionConfirm() : CommonLUBitStream(MessageType::Server::VERSION_CONFIRM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle() override;
	};

	struct DisconnectNotify : public CommonLUBitStream {
		eServerDisconnectIdentifiers disconnectID = eServerDisconnectIdentifiers::UNKNOWN_SERVER_ERROR;

		DisconnectNotify() : CommonLUBitStream(MessageType::Server::DISCONNECT_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
	};

	struct GeneralNotify : public CommonLUBitStream {
		uint32_t notifyID = 0; // only one known value: 0, which is Duplicate account login
		bool notifyUser = true;

		GeneralNotify() : CommonLUBitStream(MessageType::Server::GENERAL_NOTIFY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
	};
	
	// Non Struct functions
	void Handle(RakNet::BitStream& inStream, const SystemAddress& sysAddr);
}

