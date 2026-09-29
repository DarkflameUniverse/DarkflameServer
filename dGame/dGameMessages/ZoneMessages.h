#ifndef ZONEMESSAGES_H
#define ZONEMESSAGES_H

#include "GameMessages.h"
#include "eObjectWorldState.h"

#include <limits>
#include <string>

// Game messages for a player's zone lifecycle: loading in, zone summaries, level ups, announcements.
// Fields are listed in wire order; names follow the client (legouniverse.exe 1.10.64) where known.
namespace GameMessages {
	// Server -> client, to one client. The player is about to change zones. The client checks the map against its
	// INVALIDMAPTRANSFERLIST (civilians only, when bCheckTransferAllowed). The optional fields are written only when
	// they differ from the defaults below (the client's own invalid clone and map are 0).
	struct TransferToZone : public NetGameMsg {
		TransferToZone() : NetGameMsg(MessageType::Game::TRANSFER_TO_ZONE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bCheckTransferAllowed{ false };
		LWOCLONEID cloneID{ 0 }; // optional
		float posX{ std::numeric_limits<float>::max() }; // optional
		float posY{ std::numeric_limits<float>::max() }; // optional
		float posZ{ std::numeric_limits<float>::max() }; // optional
		float rotW{ 1.0f }; // optional
		float rotX{ 0.0f }; // optional
		float rotY{ 0.0f }; // optional
		float rotZ{ 0.0f }; // optional
		std::u16string spawnPoint{}; // u32 length prefixed
		uint8_t ucInstanceType{};
		LWOMAPID zoneID{ 0 }; // optional
	};

	// Server -> client, to one client. The transfer was checked: with no queue the client pauses the player's
	// controls, shows the target zone's loading screen and leaves gameplay; with a queue it cancels the launch
	// (LWOCharacterComponent, case TransferToZoneCheckedIM). Same layout as TransferToZone.
	struct TransferToZoneCheckedIM : public NetGameMsg {
		TransferToZoneCheckedIM() : NetGameMsg(MessageType::Game::TRANSFER_TO_ZONE_CHECKED_IM) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bIsThereaQueue{ false };
		LWOCLONEID cloneID{ 0 }; // optional
		float posX{ std::numeric_limits<float>::max() }; // optional
		float posY{ std::numeric_limits<float>::max() }; // optional
		float posZ{ std::numeric_limits<float>::max() }; // optional
		float rotW{ 1.0f }; // optional
		float rotX{ 0.0f }; // optional
		float rotY{ 0.0f }; // optional
		float rotZ{ 0.0f }; // optional
		std::u16string spawnPoint{}; // u32 length prefixed
		uint8_t ucInstanceType{};
		LWOMAPID zoneID{ 0 }; // optional
	};

	/**
	 * What live sent right before TRANSFER_TO_WORLD when a player launched to another zone (131 captured launches):
	 * player flag 32 set, TransferToZone (transfer check on), TransferToZoneCheckedIM (no queue), flag 32 cleared.
	 * No position or rotation; the clone only for properties.
	 */
	void SendZoneTransferNotice(LWOOBJID player, LWOMAPID zoneID, LWOCLONEID cloneID, const std::u16string& spawnPoint, const SystemAddress& sysAddr);

	// Client -> server. The client finished loading the zone and its player.
	struct PlayerLoaded : public NetGameMsg {
		PlayerLoaded() : NetGameMsg(MessageType::Game::PLAYER_LOADED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{};
	};

	// Client -> server. The client finished loading an object. Nothing to do.
	struct ReadyForUpdates : public NetGameMsg {
		ReadyForUpdates() : NetGameMsg(MessageType::Game::READY_FOR_UPDATES) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID objectID{};
	};

	// Server -> client, to one client. No payload.
	struct PlayerReady : public NetGameMsg {
		PlayerReady() : NetGameMsg(MessageType::Game::PLAYER_READY) {}
	};

	// Answers PlayerLoaded the way live did (235 of 237 loads): PlayerReady to the player, then to the zone control
	// object, whose client scripts wait for it. zoneControl is LWOOBJID_EMPTY when the zone has none.
	void SendPlayerReady(LWOOBJID player, LWOOBJID zoneControl, const SystemAddress& sysAddr);

	// Server -> client, to one client. No payload.
	struct RestoreToPostLoadStats : public NetGameMsg {
		RestoreToPostLoadStats() : NetGameMsg(MessageType::Game::RESTORE_TO_POST_LOAD_STATS) {}
	};

	// Server -> client, to one client. No payload.
	struct ServerDoneLoadingAllObjects : public NetGameMsg {
		ServerDoneLoadingAllObjects() : NetGameMsg(MessageType::Game::SERVER_DONE_LOADING_ALL_OBJECTS) {}
	};

	/**
	 * Ends a player's load the way live did: ServerDoneLoadingAllObjects, then PlayerReachedRespawnCheckpoint with where
	 * the player respawns in this zone (143 of 157 captured loads send the pair back to back, with a rotation).
	 * That is their saved checkpoint for the zone when they have one (DLU saves no rotation for it, so it goes
	 * unrotated), otherwise the spot and facing they loaded in at.
	 */
	void SendDoneLoading(LWOOBJID player, const NiPoint3& savedCheckpoint, const NiPoint3& spawnPosition, const NiQuaternion& spawnRotation, const SystemAddress& sysAddr);

	// Server -> client, to one client.
	struct InvalidZoneTransferList : public NetGameMsg {
		InvalidZoneTransferList() : NetGameMsg(MessageType::Game::INVALID_ZONE_TRANSFER_LIST) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::u16string customerFeedbackURL{};
		std::u16string invalidMapTransferList{};
		bool bCustomerFeedbackOnExit{};
		bool bCustomerFeedbackOnInvalidMapTransfer{};
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct DisplayZoneSummary : public NetGameMsg {
		DisplayZoneSummary() : NetGameMsg(MessageType::Game::DISPLAY_ZONE_SUMMARY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool isPropertyMap{ false };
		bool isZoneStart{ false };
		LWOOBJID sender{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server.
	struct ZoneSummaryDismissed : public NetGameMsg {
		ZoneSummaryDismissed() : NetGameMsg(MessageType::Game::ZONE_SUMMARY_DISMISSED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID playerID{};
	};

	// Client -> server. No payload. The client finished showing a level up.
	struct NotifyServerLevelProcessingComplete : public NetGameMsg {
		NotifyServerLevelProcessingComplete() : NetGameMsg(MessageType::Game::NOTIFY_SERVER_LEVEL_PROCESSING_COMPLETE) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client, to one client (UNASSIGNED broadcasts). newState is optional (a flag, then the state when it
	// isn't INWORLD), as live sent it (e.g. 80 80 00 00 00 for ATTACHED).
	struct ChangeObjectWorldState : public NetGameMsg {
		ChangeObjectWorldState() : NetGameMsg(MessageType::Game::CHANGE_OBJECT_WORLD_STATE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eObjectWorldState newState{ eObjectWorldState::INWORLD }; // optional
	};

	// Server -> client, to one client. The client looks both strings up in its locale (falling back to the text
	// itself) and shows the announcement popup. Client Deserialize: 0x00f23c50.
	struct LocalizedAnnouncementServerToSingleClient : public NetGameMsg {
		LocalizedAnnouncementServerToSingleClient() : NetGameMsg(MessageType::Game::LOCALIZED_ANNOUNCEMENT_SERVER_TO_SINGLE_CLIENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// Name-value (LDF) text: u32 character count, then that many characters plus a null terminator when not empty.
		std::u16string bodyParams{};
		bool bForceOpenChatBox{ false };
		bool bShowAnnouncement{ true };
		bool bShowInChat{ true };
		std::u16string body{};
		std::u16string title{};
		std::u16string titleParams{}; // name-value (LDF) text, as bodyParams
	};
}

#endif // ZONEMESSAGES_H
