#ifndef OBJECTMESSAGES_H
#define OBJECTMESSAGES_H

#include "GameMessages.h"
#include "eTerminateType.h"

#include <string>

// Game messages between an object's server script and its client script (fire event, notify object), and object
// interaction (use, terminate, names, debug info).
// Fields are listed in wire order; names follow the client (legouniverse.exe 1.10.64) where known.
namespace GameMessages {
	// Server -> client, to one client.
	struct FireEventClientSide : public NetGameMsg {
		FireEventClientSide() : NetGameMsg(MessageType::Game::FIRE_EVENT_CLIENT_SIDE) {}
		FireEventClientSide(const LWOOBJID _target, const std::u16string& _args, const LWOOBJID _object, const LWOOBJID _senderID) : FireEventClientSide() {
			target = _target;
			args = _args;
			object = _object;
			senderID = _senderID;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string args{};
		LWOOBJID object{};
		int64_t param1{ 0 }; // optional
		int32_t param2{ -1 }; // optional
		LWOOBJID senderID{};
	};

	// Client -> server.
	struct FireEventServerSide : public NetGameMsg {
		FireEventServerSide() : NetGameMsg(MessageType::Game::FIRE_EVENT_SERVER_SIDE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string args{};
		int32_t param1{ -1 }; // optional
		int32_t param2{ -1 }; // optional
		int32_t param3{ -1 }; // optional
		LWOOBJID senderID{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct NotifyClientObject : public NetGameMsg {
		NotifyClientObject() : NetGameMsg(MessageType::Game::NOTIFY_CLIENT_OBJECT) {}
		NotifyClientObject(const LWOOBJID _target, const std::u16string& _name, const int32_t _param1 = 0, const int32_t _param2 = 0,
			const LWOOBJID _paramObj = LWOOBJID_EMPTY, const std::string& _paramStr = "") : NotifyClientObject() {
			target = _target;
			name = _name;
			param1 = _param1;
			param2 = _param2;
			paramObj = _paramObj;
			paramStr = _paramStr;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string name{};
		int32_t param1{};
		int32_t param2{};
		LWOOBJID paramObj{};
		std::string paramStr{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts). Same layout as NotifyClientObject.
	struct NotifyClientZoneObject : public NetGameMsg {
		NotifyClientZoneObject() : NetGameMsg(MessageType::Game::NOTIFY_CLIENT_ZONE_OBJECT) {}
		NotifyClientZoneObject(const LWOOBJID _target, const std::u16string& _name, const int32_t _param1 = 0, const int32_t _param2 = 0,
			const LWOOBJID _paramObj = LWOOBJID_EMPTY, const std::string& _paramStr = "") : NotifyClientZoneObject() {
			target = _target;
			name = _name;
			param1 = _param1;
			param2 = _param2;
			paramObj = _paramObj;
			paramStr = _paramStr;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string name{};
		int32_t param1{};
		int32_t param2{};
		LWOOBJID paramObj{};
		std::string paramStr{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct NotifyObject : public NetGameMsg {
		NotifyObject() : NetGameMsg(MessageType::Game::NOTIFY_OBJECT) {}
		NotifyObject(const LWOOBJID _target, const LWOOBJID _objIDSender, const std::u16string& _name, const int32_t _param1 = 0, const int32_t _param2 = 0) : NotifyObject() {
			target = _target;
			objIDSender = _objIDSender;
			name = _name;
			param1 = _param1;
			param2 = _param2;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID objIDSender{};
		std::u16string name{};
		int32_t param1{ 0 };
		int32_t param2{ 0 };
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct ScriptNetworkVarUpdate : public NetGameMsg {
		ScriptNetworkVarUpdate() : NetGameMsg(MessageType::Game::SCRIPT_NETWORK_VAR_UPDATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string tableOfVars{}; // name-value (LDF) text
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct NotifyClientFailedPrecondition : public NetGameMsg {
		NotifyClientFailedPrecondition() : NetGameMsg(MessageType::Game::NOTIFY_CLIENT_FAILED_PRECONDITION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string failedReason{};
		int32_t preconditionID{};
	};

	// Server -> client, broadcast.
	struct TerminateInteraction : public NetGameMsg {
		TerminateInteraction() : NetGameMsg(MessageType::Game::TERMINATE_INTERACTION) {}
		TerminateInteraction(const LWOOBJID _target, const eTerminateType _type, const LWOOBJID _terminator) : TerminateInteraction() {
			target = _target;
			type = _type;
			terminator = _terminator;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID terminator{};
		eTerminateType type{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct SetName : public NetGameMsg {
		SetName() : NetGameMsg(MessageType::Game::SET_NAME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string name{};
	};

	// Client -> server.
	struct RequestUse : public NetGameMsg {
		RequestUse() : NetGameMsg(MessageType::Game::REQUEST_USE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		// Set to true if this coming from a multi-interaction UI on the client.
		bool bIsMultiInteractUse{};

		// Used only for multi-interaction
		uint32_t multiInteractID{};

		// Used only for multi-interaction, is of the enum type InteractionType
		int32_t multiInteractType{};

		LWOOBJID object{};

		bool secondary{ false };
	};
	using RequestUseEvent = NetGameMsgEvent<RequestUse>;

	// Client -> server. Developer only.
	struct RequestServerObjectInfo : public NetGameMsg {
		RequestServerObjectInfo() : NetGameMsg(MessageType::Game::REQUEST_SERVER_OBJECT_INFO, eGameMasterLevel::DEVELOPER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bVerbose{};
		LWOOBJID clientId{};
		LWOOBJID targetForReport{};
	};
	using RequestServerObjectInfoEvent = NetGameMsgEvent<RequestServerObjectInfo>;
}

#endif // OBJECTMESSAGES_H
