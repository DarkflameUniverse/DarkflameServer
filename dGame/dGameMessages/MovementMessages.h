#ifndef MOVEMENTMESSAGES_H
#define MOVEMENTMESSAGES_H

#include "GameMessages.h"
#include "NiQuaternion.h"
#include "eControlScheme.h"
#include "eCyclingMode.h"
#include "eMovementPlatformState.h"

#include <string>

enum class eCameraTargetCyclingMode : int32_t {
	ALLOW_CYCLE_TEAMMATES,
	DISALLOW_CYCLING
};

// Game messages for moving things around: teleports, moving platforms, rails, mounts and possession, jetpacks,
// ghosting reference points, the camera and the control scheme.
// Fields are listed in wire order; names follow the client (legouniverse.exe 1.10.64) where known.
namespace GameMessages {
	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct Teleport : public NetGameMsg {
		Teleport() : NetGameMsg(MessageType::Game::TELEPORT) {}
		// bIgnoreY is set when pos.y is 0, as DLU always did.
		Teleport(const LWOOBJID _target, const NiPoint3& _pos, const NiQuaternion& _rot, const bool _bSetRotation = false) : Teleport() {
			target = _target;
			bIgnoreY = _pos.y == 0.0f;
			bSetRotation = _bSetRotation;
			pos = _pos;
			rot = _rot;
		}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bIgnoreY{ true };
		bool bSetRotation{ false };
		bool bSkipAllChecks{ false };
		NiPoint3 pos{};
		bool bUseNavmesh{ false };
		// w is optional (default 1.0f), then x, y, z always.
		NiQuaternion rot{ QuatUtils::IDENTITY };
	};

	// Server -> client, broadcast. No payload.
	struct StartPathing : public NetGameMsg {
		StartPathing() : NetGameMsg(MessageType::Game::START_PATHING) {}
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct PlatformResync : public NetGameMsg {
		PlatformResync() : NetGameMsg(MessageType::Game::PLATFORM_RESYNC) {}
		// What DLU sends for a platform entity: the arguments, overridden for a few LOTs whose paths only have a
		// start and an end (special is for the FV tree platform).
		PlatformResync(const Entity& entity, bool bStopAtDesiredWaypoint = false,
			int32_t iIndex = 0, int32_t iDesiredWaypointIndex = 1, int32_t nextIndex = 1,
			eMovementPlatformState movementState = eMovementPlatformState::Moving, bool special = false);
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bReverse{ false };
		bool bStopAtDesiredWaypoint{ false };
		int32_t eCommand{ 0 };
		eMovementPlatformState eState{ eMovementPlatformState::Moving }; // written as int32_t
		int32_t eUnexpectedCommand{ 0 };
		float fIdleTimeElapsed{ 0.0f };
		float fMoveTimeElapsed{ 0.0f };
		float fPercentBetweenPoints{ 0.0f };
		int32_t iDesiredWaypointIndex{ 1 };
		int32_t iIndex{ 0 };
		int32_t iNextIndex{ 1 };
		NiPoint3 ptUnexpectedLocation{ NiPoint3Constant::ZERO };
		// optional (default identity), written x, y, z, w
		NiQuaternion qUnexpectedRotation{ QuatUtils::IDENTITY };
	};

	// Client -> server. No payload. Answered with a PlatformResync.
	struct RequestPlatformResync : public NetGameMsg {
		RequestPlatformResync() : NetGameMsg(MessageType::Game::REQUEST_PLATFORM_RESYNC) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct OrientToAngle : public NetGameMsg {
		OrientToAngle() : NetGameMsg(MessageType::Game::ORIENT_TO_ANGLE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bRelativeToCurrent{};
		float fAngle{};
	};

	// Server -> client, broadcast.
	struct LockNodeRotation : public NetGameMsg {
		LockNodeRotation() : NetGameMsg(MessageType::Game::LOCK_NODE_ROTATION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		std::string nodeName{};
	};

	// Server -> client, to one client.
	struct SetGravityScale : public NetGameMsg {
		SetGravityScale() : NetGameMsg(MessageType::Game::SET_GRAVITY_SCALE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float scale{};
	};

	// Server -> client, broadcast.
	struct SetJetPackMode : public NetGameMsg {
		SetJetPackMode() : NetGameMsg(MessageType::Game::SET_JET_PACK_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bBypassChecks{ false };
		bool bDoHover{ false };
		bool bUse{};
		int32_t effectID{ -1 }; // optional
		float fAirspeed{ 10.0f }; // optional
		float fMaxAirspeed{ 15.0f }; // optional
		float fVertVel{ 1.0f }; // optional
		int32_t iWarningEffectID{ -1 }; // optional
	};

	// Server -> client, to one client.
	struct SetPlayerControlScheme : public NetGameMsg {
		SetPlayerControlScheme() : NetGameMsg(MessageType::Game::SET_PLAYER_CONTROL_SCHEME) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bDelayCamSwitchIfInCinematic{ true };
		bool bSwitchCam{ true };
		eControlScheme iScheme{ eControlScheme::SCHEME_A }; // optional
	};

	// Server -> client, to one client.
	struct PlayerReachedRespawnCheckpoint : public NetGameMsg {
		PlayerReachedRespawnCheckpoint() : NetGameMsg(MessageType::Game::PLAYER_REACHED_RESPAWN_CHECKPOINT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		NiPoint3 pos{};
		// optional (default identity), written w, x, y, z
		NiQuaternion rot{ QuatUtils::IDENTITY };
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct SetRailMovement : public NetGameMsg {
		SetRailMovement() : NetGameMsg(MessageType::Game::SET_RAIL_MOVEMENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool pathGoForward{};
		std::u16string pathName{};
		uint32_t pathStart{};
		int32_t railActivatorComponentID{ -1 }; // optional
		LWOOBJID railActivatorObjectID{ LWOOBJID_EMPTY }; // optional
	};

	// Server -> client, to one client (UNASSIGNED broadcasts).
	struct StartRailMovement : public NetGameMsg {
		StartRailMovement() : NetGameMsg(MessageType::Game::START_RAIL_MOVEMENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bDamageImmune{ true };
		bool bNoAggro{ true };
		bool bNotifyActor{ false };
		bool bShowNameBillboard{ true };
		bool bCameraLocked{ true };
		bool bCollisionEnabled{ true };
		std::u16string loopSound{};
		bool goForward{ true };
		std::u16string pathName{};
		uint32_t pathStart{ 0 }; // optional
		int32_t railActivatorComponentID{ -1 }; // optional
		LWOOBJID railActivatorObjectID{ LWOOBJID_EMPTY }; // optional
		std::u16string startSound{};
		std::u16string stopSound{};
		bool bUseDB{ true };
	};

	// Client -> server. No payload.
	struct ClientRailMovementReady : public NetGameMsg {
		ClientRailMovementReady() : NetGameMsg(MessageType::Game::CLIENT_RAIL_MOVEMENT_READY) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server.
	struct CancelRailMovement : public NetGameMsg {
		CancelRailMovement() : NetGameMsg(MessageType::Game::CANCEL_RAIL_MOVEMENT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bImmediate{};
	};

	// Client -> server, no payload. The client's rail activator asks for its state when it is added to the world
	// (LWORailActivatorComponent::SendMessage 0x00c00da0); live answered NotifyRailActivatorStateChange to that client.
	struct RequestRailActivatorState : public NetGameMsg {
		RequestRailActivatorState() : NetGameMsg(MessageType::Game::REQUEST_RAIL_ACTIVATOR_STATE) {}
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client. The client sets rail_activator_active and updates the rail's pick type (usable or not).
	struct NotifyRailActivatorStateChange : public NetGameMsg {
		NotifyRailActivatorStateChange() : NetGameMsg(MessageType::Game::NOTIFY_RAIL_ACTOVATOR_STATE_CHANGE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bActive{ true };
	};

	// Client -> server.
	struct PlayerRailArrivedNotification : public NetGameMsg {
		PlayerRailArrivedNotification() : NetGameMsg(MessageType::Game::PLAYER_RAIL_ARRIVED_NOTIFICATION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		std::u16string pathName{};
		int32_t waypointNumber{};
	};

	// Server -> client, broadcast.
	struct SetMountInventoryID : public NetGameMsg {
		SetMountInventoryID() : NetGameMsg(MessageType::Game::SET_MOUNT_INVENTORY_ID) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID inventoryMountID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server.
	struct DismountComplete : public NetGameMsg {
		DismountComplete() : NetGameMsg(MessageType::Game::DISMOUNT_COMPLETE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID mountID{};
	};

	// Client -> server.
	struct AcknowledgePossession : public NetGameMsg {
		AcknowledgePossession() : NetGameMsg(MessageType::Game::ACKNOWLEDGE_POSSESSION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID possessedObjID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server. LWOCharacterComponent sends it on load: a scale for the distance at which objects are ghosted
	// for this player. All 239 live packets left the optional scale at its default (1), so the server keeps its own
	// ghosting distances.
	struct ModifyGhostingDistance : public NetGameMsg {
		ModifyGhostingDistance() : NetGameMsg(MessageType::Game::MODIFY_GHOSTING_DISTANCE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		float fDistanceScalar{ 1.0f }; // optional
	};

	// Client -> server.
	struct ToggleGhostReferenceOverride : public NetGameMsg {
		ToggleGhostReferenceOverride() : NetGameMsg(MessageType::Game::TOGGLE_GHOST_REFERENCE_OVERRIDE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bOverride{ false };
	};

	// Client -> server.
	struct SetGhostReferencePosition : public NetGameMsg {
		SetGhostReferencePosition() : NetGameMsg(MessageType::Game::SET_GHOST_REFERENCE_POSITION) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		NiPoint3 pos{};
	};

	// Server -> client, to one client.
	struct PlayerSetCameraCyclingMode : public NetGameMsg {
		PlayerSetCameraCyclingMode() : NetGameMsg(MessageType::Game::PLAYER_SET_CAMERA_CYCLING_MODE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bAllowCyclingWhileDeadOnly{ true };
		eCyclingMode cyclingMode{ eCyclingMode::ALLOW_CYCLE_TEAMMATES }; // optional
	};

	// Server -> client, to one client.
	struct ForceCameraTargetCycle : public NetGameMsg {
		ForceCameraTargetCycle() : NetGameMsg(MessageType::Game::FORCE_CAMERA_TARGET_CYCLE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bForceCycling{};
		eCameraTargetCyclingMode cyclingMode{ eCameraTargetCyclingMode::ALLOW_CYCLE_TEAMMATES }; // optional
		LWOOBJID optionalTargetID{};
	};
}

#endif // MOVEMENTMESSAGES_H
