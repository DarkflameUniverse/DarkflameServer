#include "SkillMessages.h"

#include "BehaviorSlot.h"
#include "BitStreamUtils.h"
#include "CDClientManager.h"
#include "CDSkillBehaviorTable.h"
#include "DestroyableComponent.h"
#include "dServer.h"
#include "eMissionTaskType.h"
#include "Entity.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"
#include "SkillComponent.h"

namespace GameMessages {
	void AddSkill::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, AICombatWeight, 0);
		bitStream.Write(bFromSkillSet);
		BitStreamUtils::WriteOptional(bitStream, castType, 0);
		BitStreamUtils::WriteOptional(bitStream, fTimeSecs, -1.0f);
		BitStreamUtils::WriteOptional(bitStream, iTimesCanCast, -1);
		bitStream.Write(skillID);
		BitStreamUtils::WriteOptional(bitStream, slotID, BehaviorSlot::Invalid);
		bitStream.Write(temporary);
	}

	bool AddSkill::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, AICombatWeight, 0));
		VALIDATE_READ(bitStream.Read(bFromSkillSet));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, castType, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fTimeSecs, -1.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iTimesCanCast, -1));
		VALIDATE_READ(bitStream.Read(skillID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, slotID, BehaviorSlot::Invalid));
		VALIDATE_READ(bitStream.Read(temporary));
		return true;
	}

	void RemoveSkill::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFromSkillSet);
		bitStream.Write(skillID);
	}

	bool RemoveSkill::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bFromSkillSet));
		VALIDATE_READ(bitStream.Read(skillID));
		return true;
	}

	void SelectSkill::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bFromSkillSet);
		bitStream.Write(skillID);
	}

	bool SelectSkill::Deserialize(RakNet::BitStream& bitStream) {
		// The old handler read nothing; keep accepting the message whatever its payload.
		if (!bitStream.Read(bFromSkillSet) || !bitStream.Read(skillID)) {
			bFromSkillSet = false;
			skillID = 0;
		}
		return true;
	}

	// Currently not actually used for our implementation, however its used right now to get around invisible inventory items in the client.
	void SelectSkill::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto var = entity.GetVar<bool>(u"dlu_first_time_load");
		if (var) {
			entity.SetVar<bool>(u"dlu_first_time_load", false);
			InventoryComponent* inventoryComponent = entity.GetComponent<InventoryComponent>();

			if (inventoryComponent) inventoryComponent->FixInvisibleItems();
		}
	}

	void StartSkill::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bUsedMouse);
		BitStreamUtils::WriteOptional(bitStream, consumableItemID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, fCasterLatency, 0.0f);
		BitStreamUtils::WriteOptional(bitStream, iCastType, 0);
		BitStreamUtils::WriteOptional(bitStream, lastClickedPosit, NiPoint3Constant::ZERO);
		bitStream.Write(optionalOriginatorID);
		BitStreamUtils::WriteOptional(bitStream, optionalTargetID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, originatorRot, QuatUtils::IDENTITY);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
		bitStream.Write(skillID);
		BitStreamUtils::WriteOptional(bitStream, uiSkillHandle, 0u);
	}

	bool StartSkill::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bUsedMouse));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, consumableItemID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fCasterLatency, 0.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iCastType, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, lastClickedPosit, NiPoint3Constant::ZERO));
		VALIDATE_READ(bitStream.Read(optionalOriginatorID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, optionalTargetID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, originatorRot, QuatUtils::IDENTITY));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		VALIDATE_READ(bitStream.Read(skillID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, uiSkillHandle, 0u));
		return true;
	}

	void StartSkill::Handle(Entity& entityRef, const SystemAddress& sysAddr) {
		Entity* entity = &entityRef;
		if (skillID == 1561 || skillID == 1562 || skillID == 1541) return;

		MissionComponent* comp = entity->GetComponent<MissionComponent>();
		if (comp) {
			comp->Progress(eMissionTaskType::USE_SKILL, skillID);
		}

		CDSkillBehaviorTable* skillTable = CDClientManager::GetTable<CDSkillBehaviorTable>();
		unsigned int behaviorId = skillTable->GetSkillByID(skillID).behaviorID;

		bool success = false;

		if (behaviorId > 0) {
			auto bs = RakNet::BitStream(reinterpret_cast<unsigned char*>(&sBitStream[0]), sBitStream.size(), false);

			auto* const skillComponent = entity->GetComponent<SkillComponent>();

			if (skillComponent) success = skillComponent->CastPlayerSkill(behaviorId, uiSkillHandle, bs, optionalTargetID, skillID);

			if (success && entity->GetCharacter()) {
				DestroyableComponent* destComp = entity->GetComponent<DestroyableComponent>();
				destComp->SetImagination(destComp->GetImagination() - skillTable->GetSkillByID(skillID).imaginationcost);
			}
		}

		if (Game::server->GetZoneID() == 1302) {
			return;
		}

		if (success) {
			// Echo the cast to every other client
			EchoStartSkill echoStartSkill;
			echoStartSkill.target = entity->GetObjectID();
			echoStartSkill.bUsedMouse = bUsedMouse;
			echoStartSkill.fCasterLatency = fCasterLatency;
			echoStartSkill.iCastType = iCastType;
			echoStartSkill.lastClickedPosit = lastClickedPosit;
			echoStartSkill.optionalOriginatorID = optionalOriginatorID;
			echoStartSkill.optionalTargetID = optionalTargetID;
			echoStartSkill.originatorRot = originatorRot;
			echoStartSkill.sBitStream = sBitStream;
			echoStartSkill.skillID = skillID;
			echoStartSkill.uiSkillHandle = uiSkillHandle;
			echoStartSkill.BroadcastExcept(entity->GetSystemAddress());
		}
	}

	void EchoStartSkill::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bUsedMouse);
		BitStreamUtils::WriteOptional(bitStream, fCasterLatency, 0.0f);
		BitStreamUtils::WriteOptional(bitStream, iCastType, 0);
		BitStreamUtils::WriteOptional(bitStream, lastClickedPosit, NiPoint3Constant::ZERO);
		bitStream.Write(optionalOriginatorID);
		BitStreamUtils::WriteOptional(bitStream, optionalTargetID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, originatorRot, QuatUtils::IDENTITY);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
		bitStream.Write(skillID);
		BitStreamUtils::WriteOptional(bitStream, uiSkillHandle, 0u);
	}

	bool EchoStartSkill::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bUsedMouse));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, fCasterLatency, 0.0f));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, iCastType, 0));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, lastClickedPosit, NiPoint3Constant::ZERO));
		VALIDATE_READ(bitStream.Read(optionalOriginatorID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, optionalTargetID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, originatorRot, QuatUtils::IDENTITY));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		VALIDATE_READ(bitStream.Read(skillID));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, uiSkillHandle, 0u));
		return true;
	}

	void SyncSkill::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bDone);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
		bitStream.Write(uiBehaviorHandle);
		bitStream.Write(uiSkillHandle);
	}

	bool SyncSkill::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bDone));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		VALIDATE_READ(bitStream.Read(uiBehaviorHandle));
		VALIDATE_READ(bitStream.Read(uiSkillHandle));
		return true;
	}

	void SyncSkill::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto bs = RakNet::BitStream(reinterpret_cast<unsigned char*>(&sBitStream[0]), sBitStream.size(), false);

		auto* const skillComponent = entity.GetComponent<SkillComponent>();

		if (skillComponent) skillComponent->SyncPlayerSkill(uiSkillHandle, uiBehaviorHandle, bs);

		// Echo the sync to every other client
		EchoSyncSkill echo;
		echo.target = entity.GetObjectID();
		echo.bDone = bDone;
		echo.sBitStream = sBitStream;
		echo.uiBehaviorHandle = uiBehaviorHandle;
		echo.uiSkillHandle = uiSkillHandle;
		echo.BroadcastExcept(sysAddr);
	}

	void EchoSyncSkill::Serialize(RakNet::BitStream& bitStream) const {
		bitStream.Write(bDone);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
		bitStream.Write(uiBehaviorHandle);
		bitStream.Write(uiSkillHandle);
	}

	bool EchoSyncSkill::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(bitStream.Read(bDone));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		VALIDATE_READ(bitStream.Read(uiBehaviorHandle));
		VALIDATE_READ(bitStream.Read(uiSkillHandle));
		return true;
	}

	void RequestServerProjectileImpact::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, i64LocalID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, i64TargetID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
	}

	bool RequestServerProjectileImpact::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64LocalID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64TargetID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		return true;
	}

	void RequestServerProjectileImpact::Handle(Entity& entity, const SystemAddress& sysAddr) {
		auto* skill_component = entity.GetComponent<SkillComponent>();

		if (skill_component != nullptr) {
			auto bs = RakNet::BitStream(reinterpret_cast<unsigned char*>(&sBitStream[0]), sBitStream.size(), false);

			skill_component->SyncPlayerProjectile(i64LocalID, bs, i64TargetID);
		}
	}

	void DoClientProjectileImpact::Serialize(RakNet::BitStream& bitStream) const {
		BitStreamUtils::WriteOptional(bitStream, i64OrgID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, i64OwnerID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteOptional(bitStream, i64TargetID, LWOOBJID_EMPTY);
		BitStreamUtils::WriteLengthPrefixed<uint32_t>(bitStream, sBitStream);
	}

	bool DoClientProjectileImpact::Deserialize(RakNet::BitStream& bitStream) {
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64OrgID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64OwnerID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadOptional(bitStream, i64TargetID, LWOOBJID_EMPTY));
		VALIDATE_READ(BitStreamUtils::ReadLengthPrefixed<uint32_t>(bitStream, sBitStream));
		return true;
	}
}
