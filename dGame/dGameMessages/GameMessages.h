#ifndef GAMEMESSAGES_H
#define GAMEMESSAGES_H

#include "dCommonVars.h"
#include <map>
#include <type_traits>
#include <string>
#include <vector>
#include "eMovementPlatformState.h"
#include "NiPoint3.h"
#include "eEndBehavior.h"
#include "eCyclingMode.h"
#include "eLootSourceType.h"
#include "Brick.h"
#include "MessageType/Game.h"
#include "eGameMasterLevel.h"
#include "LDFFormat.h"

class AMFBaseValue;
class AMFArrayValue;
class Entity;
class Item;
class User;
class Leaderboard;
class TradeItem;
class LDFBaseData;

enum class eAnimationFlags : uint32_t;

enum class eUnequippableActiveType;
enum eInventoryType : uint32_t;
enum class eGameMasterLevel : uint8_t;
enum class eMatchUpdate : int32_t;
enum class eKillType : uint32_t;
enum class eObjectWorldState : uint32_t;
enum class eTerminateType : uint32_t;
enum class eControlScheme : uint32_t;
enum class eStateChangeType : uint32_t;
enum class ePetAbilityType : uint32_t;
enum class ePetTamingNotifyType : uint32_t;
enum class eUseItemResponse : uint32_t;
enum class eQuickBuildFailReason : uint32_t;
enum class eQuickBuildState : uint32_t;
enum class BehaviorSlot : int32_t;
enum class eVendorTransactionResult : uint32_t;
enum class eReponseMoveItemBetweenInventoryTypeCode : int32_t;
enum class eMissionState : int;
enum class AiState : uint32_t;

namespace GameMessages {
	/**
	 * A server-internal event, delivered only to the handlers entities/components/scripts registered with
	 * RegisterMsg. It never goes on the wire: it has no Serialize and no way to send it to a client.
	 */
	struct GameMsg {
		GameMsg(MessageType::Game gmId) : msgId{ gmId } {}
		virtual ~GameMsg() = default;

		// Delivers the message to the handlers registered on the target entity.
		bool Send();
		bool Send(const LWOOBJID _target);

		MessageType::Game msgId;
		LWOOBJID target{ LWOOBJID_EMPTY };
	};

	/**
	 * A game message that goes on the wire, to (Serialize) or from (Deserialize + Handle) a client.
	 * It is not a GameMsg, so it cannot be delivered to local handlers by mistake. When local handlers need to
	 * observe a network message, wrap it in a NetGameMsgEvent.
	 */
	struct NetGameMsg {
		NetGameMsg(MessageType::Game gmId, eGameMasterLevel lvl) : msgId{ gmId }, requiredGmLevel{ lvl } {}
		NetGameMsg(MessageType::Game gmId) : NetGameMsg(gmId, eGameMasterLevel::CIVILIAN) {}
		virtual ~NetGameMsg() = default;

		// Sends the message to the specified client or
		// all clients if UNASSIGNED_SYSTEM_ADDRESS is specified
		void Send(const SystemAddress& sysAddr) const;

		// Sends the message to the specified client only and never broadcasts. Given UNASSIGNED_SYSTEM_ADDRESS
		// nothing is delivered (RakNet rejects a non-broadcast send without an address).
		void SendToClient(const SystemAddress& sysAddr) const;

		// Sends the message to every client except excluded (a RakNet broadcast with that address left out).
		// Used to echo a client's message to everyone else.
		void BroadcastExcept(const SystemAddress& excluded) const;

		// Writes the complete client packet (CLIENT/GAME_MSG header, target, msgId, then Serialize()) into bitStream.
		// This is exactly what Send(sysAddr) puts on the wire; tests use it to compare bytes without a server.
		void WritePacket(RakNet::BitStream& bitStream) const;

		// Reads what WritePacket writes before Serialize(): the CLIENT/GAME_MSG header, the target and the message ID.
		// Returns false if the stream is too short or holds a different kind of packet.
		static bool ReadPacketHeader(RakNet::BitStream& bitStream, LWOOBJID& target, MessageType::Game& msgId);

		virtual void Serialize(RakNet::BitStream& bitStream) const {}
		virtual bool Deserialize(RakNet::BitStream& bitStream) { return true; }

		// Called for messages received from a client, after a successful Deserialize.
		virtual void Handle(Entity& entity, const SystemAddress& sysAddr) {};

		MessageType::Game msgId;
		LWOOBJID target{ LWOOBJID_EMPTY };

		// Minimum GM level the sending client needs for Handle to be called.
		eGameMasterLevel requiredGmLevel;
	};

	/**
	 * A server-internal event carrying a network message, so entity/component handlers can react to a message
	 * received from (or about to be sent to) a client. Uses the network message's msgId for local dispatch.
	 */
	template<typename Msg>
	struct NetGameMsgEvent : public GameMsg {
		static_assert(std::is_base_of_v<NetGameMsg, Msg>, "NetGameMsgEvent carries a NetGameMsg");
		NetGameMsgEvent() : GameMsg(Msg{}.msgId) {}
		NetGameMsgEvent(const Msg& message) : GameMsg(message.msgId), msg{ message } { target = message.target; }

		Msg msg;
	};

	// Delivers a copy of a network message to the local handlers registered on msg.target (never to a client).
	// Returns whether a handler handled it. Handlers see (and may modify) only the copy.
	template<typename Msg>
	bool DeliverLocally(const Msg& msg) {
		NetGameMsgEvent<Msg> event(msg);
		return event.Send();
	}

	// Server-internal events (GameMsg). Wire messages live in the per-domain <Domain>Messages.h files.

	struct ZoneLoadedInfo : public GameMsg {
		ZoneLoadedInfo() : GameMsg(MessageType::Game::ZONE_LOADED_INFO) {}
		int32_t maxPlayers{};
	};

	struct ConfigureRacingControl : public GameMsg {
		ConfigureRacingControl() : GameMsg(MessageType::Game::CONFIGURE_RACING_CONTROL) {}
		LwoNameValue racingSettings{};
	};



	struct ActivityNotify : public GameMsg {
		ActivityNotify() : GameMsg(MessageType::Game::ACTIVITY_NOTIFY) {}

		LwoNameValue notification{};
	};


	struct ChildLoaded : public GameMsg {
		ChildLoaded() : GameMsg(MessageType::Game::CHILD_LOADED) {}

		LOT templateID{};
		LWOOBJID childID{};
	};

	struct PlayerResurrectionFinished : public GameMsg {
		PlayerResurrectionFinished() : GameMsg(MessageType::Game::PLAYER_RESURRECTION_FINISHED) {}
	};


	struct GetObjectReportInfo : public GameMsg {
		AMFArrayValue* info{};
		AMFArrayValue* subCategory{};
		bool bVerbose{};
		LWOOBJID clientID{};

		GetObjectReportInfo() : GameMsg(MessageType::Game::GET_OBJECT_REPORT_INFO) {}
	};



	struct ResetModelToDefaults : public GameMsg {
		ResetModelToDefaults() : GameMsg(MessageType::Game::RESET_MODEL_TO_DEFAULTS) {}

		bool bResetPos{ true };
		bool bResetRot{ true };
		bool bUnSmash{ true };
		bool bResetBehaviors{ true };
	};


	struct GetPosition : public GameMsg {
		GetPosition() : GameMsg(MessageType::Game::GET_POSITION) {}

		NiPoint3 pos{};
	};

	struct SetFaction : public GameMsg {
		SetFaction() : GameMsg(MessageType::Game::SET_FACTION) {}

		int32_t factionID{};

		bool bIgnoreChecks{ false };
	};


	struct GetMissionState : public GameMsg {
		GetMissionState() : GameMsg(MessageType::Game::GET_MISSION_STATE) {}

		int32_t missionID{};
		eMissionState missionState{};
		bool cooldownInfoRequested{};
		bool cooldownFinished{};
	};

	struct GetFlag : public GameMsg {
		GetFlag() : GameMsg(MessageType::Game::GET_FLAG) {}

		uint32_t flagID{};
		bool flag{};
	};

	struct GetFactionTokenType : public GameMsg {
		GetFactionTokenType() : GameMsg(MessageType::Game::GET_FACTION_TOKEN_TYPE) {}

		LOT tokenType{ LOT_NULL };
	};

	struct MissionNeedsLot : public GameMsg {
		MissionNeedsLot() : GameMsg(MessageType::Game::MISSION_NEEDS_LOT) {}

		LOT item{};
	};



	struct IsDead : public GameMsg {
		IsDead() : GameMsg(MessageType::Game::IS_DEAD) {}

		bool bDead{};
	};


	struct GetGMInvis : public GameMsg {
		GetGMInvis() : GameMsg(MessageType::Game::GET_GM_INVIS) {}

		bool bGMInvis{ false };
  };

	struct ChildRemoved : public GameMsg {
		ChildRemoved() : GameMsg(MessageType::Game::CHILD_REMOVED) {}

		LWOOBJID childID{};
	};


	struct ObjectLoaded : public GameMsg {
		ObjectLoaded() : GameMsg(MessageType::Game::OBJECT_LOADED) {}

		LWOOBJID objectID{};
		LOT lot{};
	};

	struct NotifyCombatAIStateChange : public GameMsg {
		NotifyCombatAIStateChange() : GameMsg(MessageType::Game::NOTIFY_COMBAT_AI_STATE_CHANGE) {}

		AiState newState{};
		AiState prevState{};
	};
};
#endif // GAMEMESSAGES_H
