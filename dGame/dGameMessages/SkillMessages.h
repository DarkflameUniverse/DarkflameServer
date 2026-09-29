#ifndef SKILLMESSAGES_H
#define SKILLMESSAGES_H

#include "GameMessages.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

#include <string>

enum class BehaviorSlot : int32_t;

// Game messages for skills: casting (StartSkill / SyncSkill and their echoes), projectile impacts and the
// player's skill bar. Field names follow the client (legouniverse.exe 1.10.64); fields are listed in wire order.
// sBitStream is a skill's behavior data (its own bit stream), sent as a u32 byte count and the bytes.
namespace GameMessages {
	// Server -> client, to the player only.
	struct AddSkill : public NetGameMsg {
		AddSkill() : NetGameMsg(MessageType::Game::ADD_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t AICombatWeight{ 0 }; // optional
		bool bFromSkillSet{ false };
		int32_t castType{ 0 }; // optional
		float fTimeSecs{ -1.0f }; // optional
		int32_t iTimesCanCast{ -1 }; // optional
		TSkillID skillID{};
		BehaviorSlot slotID{ static_cast<BehaviorSlot>(-1) }; // optional, BehaviorSlot::Invalid when unset
		bool temporary{ true };
	};

	// Server -> client, to the player only.
	struct RemoveSkill : public NetGameMsg {
		RemoveSkill() : NetGameMsg(MessageType::Game::REMOVE_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bFromSkillSet{ false };
		TSkillID skillID{};
	};

	// Server -> client, to the player only. The client's SkillComponent ends its running instance of the skill
	// (LWOSkillComponent::msgUncastSkill, 0x00bd86d0): live sent it for an unequipped item's equip skills.
	struct UncastSkill : public NetGameMsg {
		UncastSkill() : NetGameMsg(MessageType::Game::UNCAST_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		int32_t skillID{};
	};

	// Client -> server. A client got EchoStartSkill (or SyncSkill) from a caster it sees as dead, aimed at another
	// object (LWOSkillComponent::msgEchoStartSkill 0x00d5dc90), and tells the server through that target object.
	// The server ends the caster's skill with that handle if the caster is dead there too.
	struct CasterDead : public NetGameMsg {
		CasterDead() : NetGameMsg(MessageType::Game::CASTER_DEAD) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID i64Caster{ LWOOBJID_EMPTY }; // optional
		uint32_t uiSkillHandle{}; // optional
	};

	// Client -> server. The player picked a skill; DLU only uses it to fix invisible items on first load.
	struct SelectSkill : public NetGameMsg {
		SelectSkill() : NetGameMsg(MessageType::Game::SELECT_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		// Not read by DLU: the old handler ignored the payload, so a message without it is still accepted.
		bool bFromSkillSet{ false };
		int32_t skillID{};
	};

	// Client -> server. The player cast a skill.
	struct StartSkill : public NetGameMsg {
		StartSkill() : NetGameMsg(MessageType::Game::START_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bUsedMouse{ false };
		LWOOBJID consumableItemID{ LWOOBJID_EMPTY }; // optional
		float fCasterLatency{ 0.0f }; // optional
		int32_t iCastType{ 0 }; // optional
		NiPoint3 lastClickedPosit{ NiPoint3Constant::ZERO }; // optional
		LWOOBJID optionalOriginatorID{};
		LWOOBJID optionalTargetID{ LWOOBJID_EMPTY }; // optional
		NiQuaternion originatorRot{ QuatUtils::IDENTITY }; // optional
		std::string sBitStream{};
		TSkillID skillID{ 0 };
		uint32_t uiSkillHandle{ 0 }; // optional
	};

	// Server -> clients. A skill cast echoed to the other clients so they play it too.
	struct EchoStartSkill : public NetGameMsg {
		EchoStartSkill() : NetGameMsg(MessageType::Game::ECHO_START_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bUsedMouse{ false };
		float fCasterLatency{ 0.0f }; // optional
		int32_t iCastType{ 0 }; // optional
		NiPoint3 lastClickedPosit{ NiPoint3Constant::ZERO }; // optional
		LWOOBJID optionalOriginatorID{};
		LWOOBJID optionalTargetID{ LWOOBJID_EMPTY }; // optional
		NiQuaternion originatorRot{ QuatUtils::IDENTITY }; // optional
		std::string sBitStream{};
		TSkillID skillID{ 0 };
		uint32_t uiSkillHandle{ 0 }; // optional
	};

	// Client -> server. More behavior data for a skill that is being cast.
	struct SyncSkill : public NetGameMsg {
		SyncSkill() : NetGameMsg(MessageType::Game::SYNC_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bDone{};
		std::string sBitStream{};
		uint32_t uiBehaviorHandle{};
		uint32_t uiSkillHandle{};
	};

	// Server -> clients.
	struct EchoSyncSkill : public NetGameMsg {
		EchoSyncSkill() : NetGameMsg(MessageType::Game::ECHO_SYNC_SKILL) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bDone{};
		std::string sBitStream{};
		uint32_t uiBehaviorHandle{};
		uint32_t uiSkillHandle{};
	};

	// Client -> server. A player's projectile hit something.
	struct RequestServerProjectileImpact : public NetGameMsg {
		RequestServerProjectileImpact() : NetGameMsg(MessageType::Game::REQUEST_SERVER_PROJECTILE_IMPACT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID i64LocalID{ LWOOBJID_EMPTY }; // optional
		LWOOBJID i64TargetID{ LWOOBJID_EMPTY }; // optional
		std::string sBitStream{};
	};

	// Server -> clients. A server side projectile hit something.
	struct DoClientProjectileImpact : public NetGameMsg {
		DoClientProjectileImpact() : NetGameMsg(MessageType::Game::DO_CLIENT_PROJECTILE_IMPACT) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		LWOOBJID i64OrgID{ LWOOBJID_EMPTY }; // optional
		LWOOBJID i64OwnerID{ LWOOBJID_EMPTY }; // optional
		LWOOBJID i64TargetID{ LWOOBJID_EMPTY }; // optional
		std::string sBitStream{};
	};

	struct UseSkillSet : public NetGameMsg {
		UseSkillSet() : NetGameMsg(MessageType::Game::USE_SKILL_SET) {}
		void Serialize(RakNet::BitStream& bitStream) const override;
		bool Deserialize(RakNet::BitStream& bitStream) override;

		bool bRemove{};
		LWOOBJID possessedId{ LWOOBJID_EMPTY };
		int32_t setId{ -1 };
	};
};

#endif // SKILLMESSAGES_H
