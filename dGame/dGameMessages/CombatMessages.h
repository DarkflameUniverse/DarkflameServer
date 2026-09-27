#ifndef COMBATMESSAGES_H
#define COMBATMESSAGES_H

#include "GameMessages.h"
#include "NiPoint3.h"

#include <string>

enum class eKillType : uint32_t;
enum class eStateChangeType : uint32_t;

// Game messages for combat: death and resurrection, stuns and immunities, buffs, knockback and the bubble buff.
// Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// Unless noted, server -> client messages are sent with Send (UNASSIGNED_SYSTEM_ADDRESS broadcasts), as before.
namespace GameMessages {
	// Server -> client, broadcast.
	struct Die : public NetGameMsg {
		Die() : NetGameMsg(MessageType::Game::DIE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bClientDeath{ false };
		bool bSpawnLoot{ true };
		std::u16string deathType{}; // u32 length prefixed
		float directionRelative_AngleXZ{};
		float directionRelative_AngleY{};
		float directionRelative_Force{};
		eKillType killType{ 0 }; // optional, eKillType::VIOLENT when unset
		LWOOBJID killerID{};
		LWOOBJID lootOwnerID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server. Read with the same layout as Die, as DLU always has.
	struct RequestDie : public NetGameMsg {
		RequestDie() : NetGameMsg(MessageType::Game::REQUEST_DIE) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bClientDeath{ false };
		bool bSpawnLoot{ false };
		std::u16string deathType{}; // u32 length prefixed
		float directionRelative_AngleXZ{};
		float directionRelative_AngleY{};
		float directionRelative_Force{};
		eKillType killType{ 0 }; // optional, eKillType::VIOLENT when unset
		LWOOBJID killerID{};
		LWOOBJID lootOwnerID{ LWOOBJID_EMPTY }; // optional
	};

	// Client -> server. No payload.
	struct RequestSmashPlayer : public NetGameMsg {
		RequestSmashPlayer() : NetGameMsg(MessageType::Game::REQUEST_SMASH_PLAYER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Client -> server. The player pressed respawn. No payload.
	struct RequestResurrect : public NetGameMsg {
		RequestResurrect() : NetGameMsg(MessageType::Game::REQUEST_RESURRECT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client (broadcast) when an entity comes back to life, and client -> server when the player has
	// resurrected.
	struct Resurrect : public NetGameMsg {
		Resurrect() : NetGameMsg(MessageType::Game::RESURRECT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bRezImmediately{ false };
	};

	// Server -> client. Smashes the target into bricks without destroying it.
	struct Smash : public NetGameMsg {
		Smash() : NetGameMsg(MessageType::Game::SMASH) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bIgnoreObjectVisibility{ false };
		float force{};
		float ghostOpacity{};
		LWOOBJID killerID{};
	};

	// Server -> client. Rebuilds a smashed target over duration seconds.
	struct UnSmash : public NetGameMsg {
		UnSmash() : NetGameMsg(MessageType::Game::UN_SMASH) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID builderID{ LWOOBJID_EMPTY }; // optional
		float duration{ 3.0f }; // optional
	};

	// Server -> client, broadcast.
	struct SetResurrectRestoreValues : public NetGameMsg {
		SetResurrectRestoreValues() : NetGameMsg(MessageType::Game::SET_RESURRECT_RESTORE_VALUES) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t iArmorRestore{ -1 }; // optional
		int32_t iHealthRestore{ -1 }; // optional
		int32_t iImaginationRestore{ -1 }; // optional
	};

	// Server -> client. Sent with SendToClient: the old function never broadcast.
	struct SetPlayerAllowedRespawn : public NetGameMsg {
		SetPlayerAllowedRespawn() : NetGameMsg(MessageType::Game::SET_PLAYER_ALLOWED_RESPAWN) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool dontPromptForRespawn{};
	};

	// Server -> client, broadcast.
	struct Knockback : public NetGameMsg {
		Knockback() : NetGameMsg(MessageType::Game::KNOCKBACK) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID Caster{ LWOOBJID_EMPTY }; // optional
		LWOOBJID Originator{ LWOOBJID_EMPTY }; // optional
		int32_t iKnockBackTimeMS{ 0 }; // optional
		NiPoint3 vector{};
	};

	// Server -> client.
	struct SetStunned : public NetGameMsg {
		SetStunned() : NetGameMsg(MessageType::Game::SET_STUNNED) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID Originator{ LWOOBJID_EMPTY }; // optional
		eStateChangeType StateChangeType{};
		bool bCantAttack{ false };
		bool bCantAttackOutChangeWasApplied{ false };
		bool bCantEquip{ false };
		bool bCantEquipOutChangeWasApplied{ false };
		bool bCantInteract{ false };
		bool bCantInteractOutChangeWasApplied{ false };
		bool bCantJump{ false };
		bool bCantJumpOutChangeWasApplied{ false };
		bool bCantMove{ false };
		bool bCantMoveOutChangeWasApplied{ false };
		bool bCantTurn{ false };
		bool bCantTurnOutChangeWasApplied{ false };
		bool bCantUseItem{ false };
		bool bCantUseItemOutChangeWasApplied{ false };
		bool bDontTerminateInteract{ false };
		bool bIgnoreImmunity{ true };
	};

	// Server -> client.
	struct SetStunImmunity : public NetGameMsg {
		SetStunImmunity() : NetGameMsg(MessageType::Game::SET_STUN_IMMUNITY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID Caster{ LWOOBJID_EMPTY }; // optional
		eStateChangeType StateChangeType{};
		bool bImmuneToStunAttack{ false };
		bool bImmuneToStunEquip{ false };
		bool bImmuneToStunInteract{ false };
		bool bImmuneToStunJump{ false };
		bool bImmuneToStunMove{ false };
		bool bImmuneToStunTurn{ false };
		bool bImmuneToStunUseItem{ false };
	};

	// Server -> client.
	// WIRE FIX: the flags go on the wire in alphabetical order, as the client's Serialize writes them
	// (GameMessage::SetStatusImmunity::Serialize @ 00d8f140 in 1.10.64; field offsets named by its Flash export
	// @ 00d8f410). DLU used to write them in the declaration order below.
	struct SetStatusImmunity : public NetGameMsg {
		SetStatusImmunity() : NetGameMsg(MessageType::Game::SET_STATUS_IMMUNITY) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		eStateChangeType StateChangeType{};
		bool bImmuneToBasicAttack{ false };
		bool bImmuneToDOT{ false };
		bool bImmuneToKnockback{ false };
		bool bImmuneToInterrupt{ false };
		bool bImmuneToSpeed{ false };
		bool bImmuneToImaginationGain{ false };
		bool bImmuneToImaginationLoss{ false };
		bool bImmuneToQuickbuildInterrupt{ false };
		bool bImmuneToPullToPoint{ false };
	};

	// Server -> client.
	struct AddBuff : public NetGameMsg {
		AddBuff() : NetGameMsg(MessageType::Game::ADD_BUFF) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bAddedByTeammate{ false };
		bool bApplyOnTeammates{ false };
		bool bCancelOnDamaged{ false };
		bool bCancelOnDeath{ false };
		bool bCancelOnLogOut{ false };
		bool bCancelOnMove{ false };
		bool bCancelOnRemoveBuff{ false };
		bool bCancelOnUI{ false };
		bool bCancelOnUnEquip{ false };
		bool bCancelOnZone{ false };
		bool bIgnoreImmunities{ false };
		bool bIsImmunity{ false };
		bool bUseRefCount{ false };
		LWOOBJID i64AddedBy{ LWOOBJID_EMPTY }; // optional
		uint32_t uiBuffID{};
		uint32_t uiDurationMS{ 0 }; // optional
	};

	// Server -> client, broadcast.
	struct RemoveBuff : public NetGameMsg {
		RemoveBuff() : NetGameMsg(MessageType::Game::REMOVE_BUFF) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		// DLU always sends false: setting it makes the client ignore the message.
		bool bFromRemoveBehavior{ false };
		bool bFromUnEquip{ false };
		bool bRemoveImmunity{ false };
		uint32_t uiBuffID{};
	};

	// Server -> client.
	struct AddRunSpeedModifier : public NetGameMsg {
		AddRunSpeedModifier() : NetGameMsg(MessageType::Game::ADD_RUN_SPEED_MODIFIER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID i64Caster{ LWOOBJID_EMPTY }; // optional
		uint32_t uiModifier{ 500 }; // optional
	};

	// Server -> client.
	struct RemoveRunSpeedModifier : public NetGameMsg {
		RemoveRunSpeedModifier() : NetGameMsg(MessageType::Game::REMOVE_RUN_SPEED_MODIFIER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		uint32_t uiModifier{ 500 }; // optional
	};

	// Client -> server. The player went under water (or into skunk stink) and wants the bubble helmet.
	struct ActivateBubbleBuff : public NetGameMsg {
		ActivateBubbleBuff() : NetGameMsg(MessageType::Game::ACTIVATE_BUBBLE_BUFF) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bSpecialAnims{};
		std::u16string wszType{}; // u32 length prefixed
	};

	// Client -> server. No payload.
	struct DeactivateBubbleBuff : public NetGameMsg {
		DeactivateBubbleBuff() : NetGameMsg(MessageType::Game::DECTIVATE_BUBBLE_BUFF) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;
	};

	// Server -> client. No payload.
	struct DeactivateBubbleBuffFromServer : public NetGameMsg {
		DeactivateBubbleBuffFromServer() : NetGameMsg(MessageType::Game::DEACTIVATE_BUBBLE_BUFF_FROM_SERVER) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
	};
};

#endif // COMBATMESSAGES_H
