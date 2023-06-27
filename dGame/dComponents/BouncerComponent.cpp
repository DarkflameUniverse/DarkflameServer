#include "BouncerComponent.h"

#include "EntityManager.h"
#include "dZoneManager.h"
#include "SwitchComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "GameMessages.h"
#include "BitStream.h"
#include "eTriggerEventType.h"

BouncerComponent::BouncerComponent(Entity* parent) : Component(parent) {
	m_BounceOnCollision = false;
	m_DirtyBounceInfo = false;
}

void BouncerComponent::Startup() {
	if (m_ParentEntity->GetLOT() == 7625) LookupPetSwitch();
}

void BouncerComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(bIsInitialUpdate || m_DirtyBounceInfo);
	if (bIsInitialUpdate || m_DirtyBounceInfo) {
		outBitStream->Write(m_BounceOnCollision);
		if (!bIsInitialUpdate) m_DirtyBounceInfo = false;
	}
}

void BouncerComponent::SetBounceOnCollision(bool value) {
	if (m_BounceOnCollision == value) return;
	m_BounceOnCollision = value;
	m_DirtyBounceInfo = true;
}

void BouncerComponent::SetBouncerEnabled(bool value) {
	m_BounceOnCollision = value;

	GameMessages::SendBouncerActiveStatus(m_ParentEntity->GetObjectID(), value, UNASSIGNED_SYSTEM_ADDRESS);

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	if (value) {
		m_ParentEntity->TriggerEvent(eTriggerEventType::PET_ON_SWITCH, m_ParentEntity);
		GameMessages::SendPlayFXEffect(m_ParentEntity->GetObjectID(), 1513, u"create", "PetOnSwitch");
	} else {
		m_ParentEntity->TriggerEvent(eTriggerEventType::PET_OFF_SWITCH, m_ParentEntity);
		GameMessages::SendStopFXEffect(m_ParentEntity, true, "PetOnSwitch");
	}
}

void BouncerComponent::LookupPetSwitch() {
	const auto& groups = m_ParentEntity->GetGroups();

	for (const auto& group : groups) {
		const auto& entities = EntityManager::Instance()->GetEntitiesInGroup(group);

		for (auto* entity : entities) {
			auto* switchComponent = entity->GetComponent<SwitchComponent>();

			if (!switchComponent) continue;
			switchComponent->SetPetBouncer(this);

			m_DirtyBounceInfo = true;

			EntityManager::Instance()->SerializeEntity(m_ParentEntity);

			Game::logger->Log("BouncerComponent", "Loaded bouncer %i:%llu", m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID());
			return;
		}
	}

	float retryTime = 0.5f;
	Game::logger->Log("BouncerComponent", "Failed to load pet bouncer for %i:%llu, trying again in %f seconds", m_ParentEntity->GetLOT(), m_ParentEntity->GetObjectID(), retryTime);

	m_ParentEntity->AddCallbackTimer(retryTime, [this]() {
		LookupPetSwitch();
		});
}
