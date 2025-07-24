#include "ControllablePhysicsComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "Logger.h"
#include "Game.h"

#include "dpWorld.h"
#include "dpEntity.h"
#include "CDPhysicsComponentTable.h"
#include "CDComponentsRegistryTable.h"
#include "CDClientManager.h"
#include "EntityManager.h"
#include "Character.h"
#include "dZoneManager.h"
#include "LevelProgressionComponent.h"
#include "eStateChangeType.h"
#include "StringifiedEnum.h"
#include "Amf3.h"

ControllablePhysicsComponent::ControllablePhysicsComponent(Entity* entity, int32_t componentId) : PhysicsComponent(entity, componentId) {
	RegisterMsg(MessageType::Game::GET_OBJECT_REPORT_INFO, this, &ControllablePhysicsComponent::OnGetObjectReportInfo);

	m_Velocity = {};
	m_AngularVelocity = {};
	m_InJetpackMode = false;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_dpEntity = nullptr;
	m_Static = false;
	m_SpeedMultiplier = 1;
	m_GravityScale = 1;
	m_DirtyCheats = false;

	m_DirtyEquippedItemInfo = true;
	m_PickupRadius = 0.0f;

	m_DirtyBubble = false;
	m_IsInBubble = false;
	m_SpecialAnims = false;
	m_BubbleType = eBubbleType::DEFAULT;

	m_IsTeleporting = false;

	m_ImmuneToStunAttackCount = 0;
	m_ImmuneToStunEquipCount = 0;
	m_ImmuneToStunInteractCount = 0;
	m_ImmuneToStunJumpCount = 0;
	m_ImmuneToStunMoveCount = 0;
	m_ImmuneToStunTurnCount = 0;
	m_ImmuneToStunUseItemCount = 0;

	if (entity->GetLOT() != 1) // Other physics entities we care about will be added by BaseCombatAI
		return;

	if (entity->GetLOT() == 1) {
		LOG("Using patch to load minifig physics");

		float radius = 1.5f;
		m_dpEntity = new dpEntity(m_Parent->GetObjectID(), radius, false);
		m_dpEntity->SetCollisionGroup(COLLISION_GROUP_DYNAMIC | COLLISION_GROUP_FRIENDLY);
		dpWorld::AddEntity(m_dpEntity);
	}
}

ControllablePhysicsComponent::~ControllablePhysicsComponent() {
	if (m_dpEntity) {
		dpWorld::RemoveEntity(m_dpEntity);
	}
}

void ControllablePhysicsComponent::Update(float deltaTime) {

}

void ControllablePhysicsComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	//If this is a creation, then we assume the position is dirty, even when it isn't.
	//This is because new clients will still need to receive the position.
	//if (bIsInitialUpdate) m_DirtyPosition = true;

	if (bIsInitialUpdate) {
		outBitStream.Write(m_InJetpackMode);
		if (m_InJetpackMode) {
			outBitStream.Write(m_JetpackEffectID);
			outBitStream.Write(m_JetpackFlying);
			outBitStream.Write(m_JetpackBypassChecks);
		}

		outBitStream.Write1(); // always write these on construction
		outBitStream.Write(m_ImmuneToStunMoveCount);
		outBitStream.Write(m_ImmuneToStunJumpCount);
		outBitStream.Write(m_ImmuneToStunTurnCount);
		outBitStream.Write(m_ImmuneToStunAttackCount);
		outBitStream.Write(m_ImmuneToStunUseItemCount);
		outBitStream.Write(m_ImmuneToStunEquipCount);
		outBitStream.Write(m_ImmuneToStunInteractCount);
	}

	outBitStream.Write(m_DirtyCheats || bIsInitialUpdate);
	if (m_DirtyCheats || bIsInitialUpdate) {
		outBitStream.Write(m_GravityScale);
		outBitStream.Write(m_SpeedMultiplier);

		if (!bIsInitialUpdate) m_DirtyCheats = false;
	}

	outBitStream.Write(m_DirtyEquippedItemInfo || bIsInitialUpdate);
	if (m_DirtyEquippedItemInfo || bIsInitialUpdate) {
		outBitStream.Write(m_PickupRadius);
		outBitStream.Write(m_InJetpackMode);

		if (!bIsInitialUpdate) m_DirtyEquippedItemInfo = false;
	}

	outBitStream.Write(m_DirtyBubble || bIsInitialUpdate);
	if (m_DirtyBubble || bIsInitialUpdate) {
		outBitStream.Write(m_IsInBubble);
		if (m_IsInBubble) {
			outBitStream.Write(m_BubbleType);
			outBitStream.Write(m_SpecialAnims);
		}

		if (!bIsInitialUpdate) m_DirtyBubble = false;
	}

	outBitStream.Write(m_DirtyPosition || bIsInitialUpdate);
	if (m_DirtyPosition || bIsInitialUpdate) {
		outBitStream.Write(m_Position.x);
		outBitStream.Write(m_Position.y);
		outBitStream.Write(m_Position.z);

		outBitStream.Write(m_Rotation.x);
		outBitStream.Write(m_Rotation.y);
		outBitStream.Write(m_Rotation.z);
		outBitStream.Write(m_Rotation.w);

		outBitStream.Write(m_IsOnGround);
		outBitStream.Write(m_IsOnRail);

		bool isNotZero = m_Velocity != NiPoint3Constant::ZERO;
		outBitStream.Write(isNotZero);
		if (isNotZero) {
			outBitStream.Write(m_Velocity.x);
			outBitStream.Write(m_Velocity.y);
			outBitStream.Write(m_Velocity.z);
		}

		isNotZero = m_AngularVelocity != NiPoint3Constant::ZERO;
		outBitStream.Write(isNotZero);
		if (isNotZero) {
			outBitStream.Write(m_AngularVelocity.x);
			outBitStream.Write(m_AngularVelocity.y);
			outBitStream.Write(m_AngularVelocity.z);
		}

		outBitStream.Write0(); // local_space_info, always zero for now.

		if (!bIsInitialUpdate) {
			m_DirtyPosition = false;
			outBitStream.Write(m_IsTeleporting);
			m_IsTeleporting = false;
		}
	}
}

void ControllablePhysicsComponent::LoadFromXml(const tinyxml2::XMLDocument& doc) {
	auto* character = doc.FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		LOG("Failed to find char tag!");
		return;
	}

	m_Parent->GetCharacter()->LoadXmlRespawnCheckpoints();

	character->QueryAttribute("lzx", &m_Position.x);
	character->QueryAttribute("lzy", &m_Position.y);
	character->QueryAttribute("lzz", &m_Position.z);
	character->QueryAttribute("lzrx", &m_Rotation.x);
	character->QueryAttribute("lzry", &m_Rotation.y);
	character->QueryAttribute("lzrz", &m_Rotation.z);
	character->QueryAttribute("lzrw", &m_Rotation.w);

	m_DirtyPosition = true;
}

void ControllablePhysicsComponent::UpdateXml(tinyxml2::XMLDocument& doc) {
	tinyxml2::XMLElement* character = doc.FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		LOG("Failed to find char tag while updating XML!");
		return;
	}

	auto zoneInfo = Game::zoneManager->GetZone()->GetZoneID();

	if (zoneInfo.GetMapID() != 0 && zoneInfo.GetCloneID() == 0 && !Game::zoneManager->GetDisableSaveLocation()) {
		character->SetAttribute("lzx", m_Position.x);
		character->SetAttribute("lzy", m_Position.y);
		character->SetAttribute("lzz", m_Position.z);
		character->SetAttribute("lzrx", m_Rotation.x);
		character->SetAttribute("lzry", m_Rotation.y);
		character->SetAttribute("lzrz", m_Rotation.z);
		character->SetAttribute("lzrw", m_Rotation.w);
	}
}

void ControllablePhysicsComponent::SetPosition(const NiPoint3& pos) {
	if (m_Static) return;
	PhysicsComponent::SetPosition(pos);
	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void ControllablePhysicsComponent::SetRotation(const NiQuaternion& rot) {
	if (m_Static) return;
	PhysicsComponent::SetRotation(rot);
	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}

void ControllablePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	if (m_Static || m_Velocity == vel) return;

	m_Velocity = vel;
	m_DirtyPosition = true;

	if (m_dpEntity) m_dpEntity->SetVelocity(vel);
}

void ControllablePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	if (m_Static || m_AngularVelocity == vel) return;

	m_AngularVelocity = vel;
	m_DirtyPosition = true;
}

void ControllablePhysicsComponent::SetIsOnGround(bool val) {
	if (m_IsOnGround == val) return;
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void ControllablePhysicsComponent::SetIsOnRail(bool val) {
	if (m_IsOnRail == val) return;
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void ControllablePhysicsComponent::SetDirtyPosition(bool val) {
	m_DirtyPosition = val;
}
void ControllablePhysicsComponent::AddPickupRadiusScale(float value) {
	m_ActivePickupRadiusScales.push_back(value);
	if (value > m_PickupRadius) {
		m_PickupRadius = value;
		m_DirtyEquippedItemInfo = true;
	}
}

void ControllablePhysicsComponent::RemovePickupRadiusScale(float value) {
	// Attempt to remove pickup radius from active radii
	const auto pos = std::find(m_ActivePickupRadiusScales.begin(), m_ActivePickupRadiusScales.end(), value);
	if (pos != m_ActivePickupRadiusScales.end()) {
		m_ActivePickupRadiusScales.erase(pos);
	} else {
		LOG_DEBUG("Warning: Could not find pickup radius %f in list of active radii.  List has %i active radii.", value, m_ActivePickupRadiusScales.size());
		return;
	}

	// Recalculate pickup radius since we removed one by now
	m_PickupRadius = 0.0f;
	m_DirtyEquippedItemInfo = true;
	for (uint32_t i = 0; i < m_ActivePickupRadiusScales.size(); i++) {
		auto candidateRadius = m_ActivePickupRadiusScales[i];
		if (m_PickupRadius < candidateRadius) m_PickupRadius = candidateRadius;
	}
	Game::entityManager->SerializeEntity(m_Parent);
}

void ControllablePhysicsComponent::AddSpeedboost(float value) {
	m_ActiveSpeedBoosts.push_back(value);
	m_SpeedBoost = value;
	SetSpeedMultiplier(value / 500.0f); // 500 being the base speed
}

void ControllablePhysicsComponent::RemoveSpeedboost(float value) {
	const auto pos = std::find(m_ActiveSpeedBoosts.begin(), m_ActiveSpeedBoosts.end(), value);
	if (pos != m_ActiveSpeedBoosts.end()) {
		m_ActiveSpeedBoosts.erase(pos);
	} else {
		LOG_DEBUG("Warning: Could not find speedboost %f in list of active speedboosts.  List has %i active speedboosts.", value, m_ActiveSpeedBoosts.size());
		return;
	}

	// Recalculate speedboost since we removed one
	m_SpeedBoost = 0.0f;
	if (m_ActiveSpeedBoosts.empty()) { // no active speed boosts left, so return to base speed
		auto* levelProgressionComponent = m_Parent->GetComponent<LevelProgressionComponent>();
		if (levelProgressionComponent) m_SpeedBoost = levelProgressionComponent->GetSpeedBase();
	} else { // Used the last applied speedboost
		m_SpeedBoost = m_ActiveSpeedBoosts.back();
	}
	SetSpeedMultiplier(m_SpeedBoost / 500.0f); // 500 being the base speed
	Game::entityManager->SerializeEntity(m_Parent);
}

void ControllablePhysicsComponent::ActivateBubbleBuff(eBubbleType bubbleType, bool specialAnims) {
	if (m_IsInBubble) {
		LOG("Already in bubble");
		return;
	}
	m_BubbleType = bubbleType;
	m_IsInBubble = true;
	m_DirtyBubble = true;
	m_SpecialAnims = specialAnims;
	Game::entityManager->SerializeEntity(m_Parent);
}

void ControllablePhysicsComponent::DeactivateBubbleBuff() {
	m_DirtyBubble = true;
	m_IsInBubble = false;
	Game::entityManager->SerializeEntity(m_Parent);
};

void ControllablePhysicsComponent::SetStunImmunity(
	const eStateChangeType state,
	const LWOOBJID originator,
	const bool bImmuneToStunAttack,
	const bool bImmuneToStunEquip,
	const bool bImmuneToStunInteract,
	const bool bImmuneToStunJump,
	const bool bImmuneToStunMove,
	const bool bImmuneToStunTurn,
	const bool bImmuneToStunUseItem) {

	if (state == eStateChangeType::POP) {
		if (bImmuneToStunAttack && m_ImmuneToStunAttackCount > 0) 		m_ImmuneToStunAttackCount -= 1;
		if (bImmuneToStunEquip && m_ImmuneToStunEquipCount > 0) 		m_ImmuneToStunEquipCount -= 1;
		if (bImmuneToStunInteract && m_ImmuneToStunInteractCount > 0) 	m_ImmuneToStunInteractCount -= 1;
		if (bImmuneToStunJump && m_ImmuneToStunJumpCount > 0) 			m_ImmuneToStunJumpCount -= 1;
		if (bImmuneToStunMove && m_ImmuneToStunMoveCount > 0) 			m_ImmuneToStunMoveCount -= 1;
		if (bImmuneToStunTurn && m_ImmuneToStunTurnCount > 0) 			m_ImmuneToStunTurnCount -= 1;
		if (bImmuneToStunUseItem && m_ImmuneToStunUseItemCount > 0) 	m_ImmuneToStunUseItemCount -= 1;
	} else if (state == eStateChangeType::PUSH) {
		if (bImmuneToStunAttack) 	m_ImmuneToStunAttackCount += 1;
		if (bImmuneToStunEquip) 	m_ImmuneToStunEquipCount += 1;
		if (bImmuneToStunInteract) 	m_ImmuneToStunInteractCount += 1;
		if (bImmuneToStunJump) 		m_ImmuneToStunJumpCount += 1;
		if (bImmuneToStunMove) 		m_ImmuneToStunMoveCount += 1;
		if (bImmuneToStunTurn) 		m_ImmuneToStunTurnCount += 1;
		if (bImmuneToStunUseItem)	m_ImmuneToStunUseItemCount += 1;
	}

	GameMessages::SendSetStunImmunity(
		m_Parent->GetObjectID(), state, m_Parent->GetSystemAddress(), originator,
		bImmuneToStunAttack,
		bImmuneToStunEquip,
		bImmuneToStunInteract,
		bImmuneToStunJump,
		bImmuneToStunMove,
		bImmuneToStunTurn,
		bImmuneToStunUseItem
	);
}

bool ControllablePhysicsComponent::OnGetObjectReportInfo(GameMessages::GameMsg& msg) {
	PhysicsComponent::OnGetObjectReportInfo(msg);
	auto& reportInfo = static_cast<GameMessages::GetObjectReportInfo&>(msg);
	auto& info = reportInfo.subCategory->PushDebug("Controllable Info");

	auto& vel = info.PushDebug("Velocity");
	vel.PushDebug<AMFDoubleValue>("x") = m_Velocity.x;
	vel.PushDebug<AMFDoubleValue>("y") = m_Velocity.y;
	vel.PushDebug<AMFDoubleValue>("z") = m_Velocity.z;

	auto& angularVelocity = info.PushDebug("Angular Velocity");
	angularVelocity.PushDebug<AMFDoubleValue>("x") = m_AngularVelocity.x;
	angularVelocity.PushDebug<AMFDoubleValue>("y") = m_AngularVelocity.y;
	angularVelocity.PushDebug<AMFDoubleValue>("z") = m_AngularVelocity.z;

	info.PushDebug<AMFBoolValue>("Is On Ground") = m_IsOnGround;
	info.PushDebug<AMFBoolValue>("Is On Rail") = m_IsOnRail;
	info.PushDebug<AMFBoolValue>("Is In Jetpack Mode") = m_InJetpackMode;
	info.PushDebug<AMFBoolValue>("Is Jetpack Flying") = m_JetpackFlying;
	info.PushDebug<AMFBoolValue>("Is Bypassing Jetpack Checks") = m_JetpackBypassChecks;
	info.PushDebug<AMFIntValue>("Jetpack Effect ID") = m_JetpackEffectID;
	info.PushDebug<AMFDoubleValue>("Speed Multiplier") = m_SpeedMultiplier;
	info.PushDebug<AMFDoubleValue>("Gravity Scale") = m_GravityScale;
	info.PushDebug<AMFBoolValue>("Is Static") = m_Static;

	auto& pickupRadii = info.PushDebug("Active Pickup Radius Scales");

	size_t i = 0;
	for (const auto& scale : m_ActivePickupRadiusScales) {
		pickupRadii.PushDebug<AMFStringValue>(std::to_string(i++) + " " + std::to_string(scale)) = "";
	}

	info.PushDebug<AMFDoubleValue>("Largest Pickup Radius") = m_PickupRadius;
	info.PushDebug<AMFBoolValue>("Is Teleporting") = m_IsTeleporting;

	auto& activeSpeedBoosts = info.PushDebug("Active Speed Boosts");
	i = 0;
	for (const auto& boost : m_ActiveSpeedBoosts) {
		activeSpeedBoosts.PushDebug<AMFStringValue>(std::to_string(i++) + " " + std::to_string(boost)) = "";
	}

	info.PushDebug<AMFDoubleValue>("Speed Boost") = m_SpeedBoost;
	info.PushDebug<AMFBoolValue>("Is In Bubble") = m_IsInBubble;
	info.PushDebug<AMFStringValue>("Bubble Type") = StringifiedEnum::ToString(m_BubbleType).data();
	info.PushDebug<AMFBoolValue>("Special Anims") = m_SpecialAnims;
	info.PushDebug<AMFIntValue>("Immune To Stun Attack Count") = m_ImmuneToStunAttackCount;
	info.PushDebug<AMFIntValue>("Immune To Stun Equip Count") = m_ImmuneToStunEquipCount;
	info.PushDebug<AMFIntValue>("Immune To Stun Interact Count") = m_ImmuneToStunInteractCount;
	info.PushDebug<AMFIntValue>("Immune To Stun Jump Count") = m_ImmuneToStunJumpCount;
	info.PushDebug<AMFIntValue>("Immune To Stun Move Count") = m_ImmuneToStunMoveCount;
	info.PushDebug<AMFIntValue>("Immune To Stun Turn Count") = m_ImmuneToStunTurnCount;
	info.PushDebug<AMFIntValue>("Immune To Stun UseItem Count") = m_ImmuneToStunUseItemCount;
	return true;
}
