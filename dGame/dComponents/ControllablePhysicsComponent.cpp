#include "ControllablePhysicsComponent.h"
#include "Entity.h"
#include "BitStream.h"
#include "dLogger.h"
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

ControllablePhysicsComponent::ControllablePhysicsComponent(Entity* entity) : Component(entity) {
	m_Position = {};
	m_Rotation = NiQuaternion::IDENTITY;
	m_Velocity = {};
	m_AngularVelocity = {};
	m_InJetpackMode = false;
	m_IsOnGround = true;
	m_IsOnRail = false;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;
	m_DirtyAngularVelocity = true;
	m_dpEntity = nullptr;
	m_Static = false;
	m_SpeedMultiplier = 1;
	m_GravityScale = 1;
	m_DirtyCheats = false;
	m_IgnoreMultipliers = false;

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
		Game::logger->Log("ControllablePhysicsComponent", "Using patch to load minifig physics");

		float radius = 1.5f;
		m_dpEntity = new dpEntity(m_Parent->GetObjectID(), radius, false);
		m_dpEntity->SetCollisionGroup(COLLISION_GROUP_DYNAMIC | COLLISION_GROUP_FRIENDLY);
		dpWorld::Instance().AddEntity(m_dpEntity);
	}
}

ControllablePhysicsComponent::~ControllablePhysicsComponent() {
	if (m_dpEntity) {
		dpWorld::Instance().RemoveEntity(m_dpEntity);
	}
}

void ControllablePhysicsComponent::Update(float deltaTime) {

}

void ControllablePhysicsComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	//If this is a creation, then we assume the position is dirty, even when it isn't.
	//This is because new clients will still need to receive the position.
	//if (bIsInitialUpdate) m_DirtyPosition = true;

	if (bIsInitialUpdate) {
		outBitStream->Write(m_InJetpackMode);
		if (m_InJetpackMode) {
			outBitStream->Write(m_JetpackEffectID);
			outBitStream->Write(m_JetpackFlying);
			outBitStream->Write(m_JetpackBypassChecks);
		}

		outBitStream->Write1(); // always write these on construction
		outBitStream->Write(m_ImmuneToStunMoveCount);
		outBitStream->Write(m_ImmuneToStunJumpCount);
		outBitStream->Write(m_ImmuneToStunTurnCount);
		outBitStream->Write(m_ImmuneToStunAttackCount);
		outBitStream->Write(m_ImmuneToStunUseItemCount);
		outBitStream->Write(m_ImmuneToStunEquipCount);
		outBitStream->Write(m_ImmuneToStunInteractCount);
	}

	if (m_IgnoreMultipliers) m_DirtyCheats = false;

	outBitStream->Write(m_DirtyCheats);
	if (m_DirtyCheats) {
		outBitStream->Write(m_GravityScale);
		outBitStream->Write(m_SpeedMultiplier);

		m_DirtyCheats = false;
	}

	outBitStream->Write(m_DirtyEquippedItemInfo);
	if (m_DirtyEquippedItemInfo) {
		outBitStream->Write(m_PickupRadius);
		outBitStream->Write(m_InJetpackMode);
		m_DirtyEquippedItemInfo = false;
	}

	outBitStream->Write(m_DirtyBubble);
	if (m_DirtyBubble) {
		outBitStream->Write(m_IsInBubble);
		if (m_IsInBubble) {
			outBitStream->Write(m_BubbleType);
			outBitStream->Write(m_SpecialAnims);
		}
		m_DirtyBubble = false;
	}

	outBitStream->Write(m_DirtyPosition || bIsInitialUpdate);
	if (m_DirtyPosition || bIsInitialUpdate) {
		outBitStream->Write(m_Position.x);
		outBitStream->Write(m_Position.y);
		outBitStream->Write(m_Position.z);

		outBitStream->Write(m_Rotation.x);
		outBitStream->Write(m_Rotation.y);
		outBitStream->Write(m_Rotation.z);
		outBitStream->Write(m_Rotation.w);

		outBitStream->Write(m_IsOnGround);
		outBitStream->Write(m_IsOnRail);

		outBitStream->Write(m_DirtyVelocity);
		if (m_DirtyVelocity) {
			outBitStream->Write(m_Velocity.x);
			outBitStream->Write(m_Velocity.y);
			outBitStream->Write(m_Velocity.z);
		}

		outBitStream->Write(m_DirtyAngularVelocity);
		if (m_DirtyAngularVelocity) {
			outBitStream->Write(m_AngularVelocity.x);
			outBitStream->Write(m_AngularVelocity.y);
			outBitStream->Write(m_AngularVelocity.z);
		}

		outBitStream->Write0();
	}

	if (!bIsInitialUpdate) {
		outBitStream->Write(m_IsTeleporting);
		m_IsTeleporting = false;
	}
}

void ControllablePhysicsComponent::LoadFromXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* character = doc->FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		Game::logger->Log("ControllablePhysicsComponent", "Failed to find char tag!");
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

void ControllablePhysicsComponent::ResetFlags() {
	m_DirtyAngularVelocity = false;
	m_DirtyPosition = false;
	m_DirtyVelocity = false;
}

void ControllablePhysicsComponent::UpdateXml(tinyxml2::XMLDocument* doc) {
	tinyxml2::XMLElement* character = doc->FirstChildElement("obj")->FirstChildElement("char");
	if (!character) {
		Game::logger->Log("ControllablePhysicsComponent", "Failed to find char tag while updating XML!");
		return;
	}

	auto zoneInfo = dZoneManager::Instance()->GetZone()->GetZoneID();

	if (zoneInfo.GetMapID() != 0 && zoneInfo.GetCloneID() == 0) {
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
	if (m_Static) {
		return;
	}

	m_Position.x = pos.x;
	m_Position.y = pos.y;
	m_Position.z = pos.z;
	m_DirtyPosition = true;

	if (m_dpEntity) m_dpEntity->SetPosition(pos);
}

void ControllablePhysicsComponent::SetRotation(const NiQuaternion& rot) {
	if (m_Static) {
		return;
	}

	m_Rotation = rot;
	m_DirtyPosition = true;

	if (m_dpEntity) m_dpEntity->SetRotation(rot);
}

void ControllablePhysicsComponent::SetVelocity(const NiPoint3& vel) {
	if (m_Static) {
		return;
	}

	m_Velocity = vel;
	m_DirtyPosition = true;
	m_DirtyVelocity = true;

	if (m_dpEntity) m_dpEntity->SetVelocity(vel);
}

void ControllablePhysicsComponent::SetAngularVelocity(const NiPoint3& vel) {
	if (m_Static) {
		return;
	}

	m_AngularVelocity = vel;
	m_DirtyPosition = true;
	m_DirtyAngularVelocity = true;
}

void ControllablePhysicsComponent::SetIsOnGround(bool val) {
	m_DirtyPosition = true;
	m_IsOnGround = val;
}

void ControllablePhysicsComponent::SetIsOnRail(bool val) {
	m_DirtyPosition = true;
	m_IsOnRail = val;
}

void ControllablePhysicsComponent::SetDirtyPosition(bool val) {
	m_DirtyPosition = val;
}

void ControllablePhysicsComponent::SetDirtyVelocity(bool val) {
	m_DirtyVelocity = val;
}

void ControllablePhysicsComponent::SetDirtyAngularVelocity(bool val) {
	m_DirtyAngularVelocity = val;
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
		Game::logger->LogDebug("ControllablePhysicsComponent", "Warning: Could not find pickup radius %f in list of active radii.  List has %i active radii.", value, m_ActivePickupRadiusScales.size());
		return;
	}

	// Recalculate pickup radius since we removed one by now
	m_PickupRadius = 0.0f;
	m_DirtyEquippedItemInfo = true;
	for (uint32_t i = 0; i < m_ActivePickupRadiusScales.size(); i++) {
		auto candidateRadius = m_ActivePickupRadiusScales[i];
		if (m_PickupRadius < candidateRadius) m_PickupRadius = candidateRadius;
	}
	EntityManager::Instance()->SerializeEntity(m_Parent);
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
		Game::logger->LogDebug("ControllablePhysicsComponent", "Warning: Could not find speedboost %f in list of active speedboosts.  List has %i active speedboosts.", value, m_ActiveSpeedBoosts.size());
		return;
	}

	// Recalculate speedboost since we removed one
	m_SpeedBoost = 0.0f;
	if (m_ActiveSpeedBoosts.size() == 0) { // no active speed boosts left, so return to base speed
		auto* levelProgressionComponent = m_Parent->GetComponent<LevelProgressionComponent>();
		if (levelProgressionComponent) m_SpeedBoost = levelProgressionComponent->GetSpeedBase();
	} else { // Used the last applied speedboost
		m_SpeedBoost = m_ActiveSpeedBoosts.back();
	}
	SetSpeedMultiplier(m_SpeedBoost / 500.0f); // 500 being the base speed
	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ControllablePhysicsComponent::ActivateBubbleBuff(eBubbleType bubbleType, bool specialAnims){
	if (m_IsInBubble) {
		Game::logger->Log("ControllablePhysicsComponent", "Already in bubble");
		return;
	}
	m_BubbleType = bubbleType;
	m_IsInBubble = true;
	m_DirtyBubble = true;
	m_SpecialAnims = specialAnims;
	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void ControllablePhysicsComponent::DeactivateBubbleBuff(){
	m_DirtyBubble = true;
	m_IsInBubble = false;
	EntityManager::Instance()->SerializeEntity(m_Parent);
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
	const bool bImmuneToStunUseItem){

	if (state == eStateChangeType::POP){
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

void ControllablePhysicsComponent::AddFallSpeed(float value) {
	m_ActiveFallSpeeds.push_back(value);
	m_FallSpeed = value;
	SetGravityScale(value);
}

void ControllablePhysicsComponent::RemoveFallSpeed(float value) {
	const auto pos = std::find(m_ActiveFallSpeeds.begin(), m_ActiveFallSpeeds.end(), value);
	if (pos != m_ActiveFallSpeeds.end()) {
		m_ActiveFallSpeeds.erase(pos);
	} else {
		Game::logger->LogDebug("ControllablePhysicsComponent", "Warning: Could not find FallSpeed %f in list of active FallSpeeds.  List has %i active FallSpeeds.", value, m_ActiveFallSpeeds.size());
		return;
	}

	// Recalculate FallSpeed since we removed one
	m_FallSpeed = 0.0f;
	if (m_ActiveFallSpeeds.size() == 0) { // no active fall speeds left, so return to base gravity
		m_FallSpeed = 1;
	} else { // Used the last applied FallSpeed
		m_FallSpeed = m_ActiveFallSpeeds.back();
	}
	SetSpeedMultiplier(m_FallSpeed); // 500 being the base speed
	EntityManager::Instance()->SerializeEntity(m_Parent);
}
