#include "BouncerComponent.h"

#include "EntityManager.h"
#include "dZoneManager.h"
#include "SwitchComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "GameMessages.h"
#include <BitStream.h>
#include "eTriggerEventType.h"

BouncerComponent::BouncerComponent(Entity* parent) : Component(parent) {
	m_PetEnabled = false;
	m_PetBouncerEnabled = false;
	m_PetSwitchLoaded = false;

	if (parent->GetLOT() == 7625) {
		LookupPetSwitch();
	}
}

BouncerComponent::~BouncerComponent() {
}

void BouncerComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_PetEnabled);
	if (m_PetEnabled) {
		outBitStream->Write(m_PetBouncerEnabled);
	}
}

Entity* BouncerComponent::GetParentEntity() const {
	return m_OwningEntity;
}

void BouncerComponent::SetPetEnabled(bool value) {
	m_PetEnabled = value;

	EntityManager::Instance()->SerializeEntity(m_OwningEntity);
}

void BouncerComponent::SetPetBouncerEnabled(bool value) {
	m_PetBouncerEnabled = value;

	GameMessages::SendBouncerActiveStatus(m_OwningEntity->GetObjectID(), value, UNASSIGNED_SYSTEM_ADDRESS);

	EntityManager::Instance()->SerializeEntity(m_OwningEntity);

	if (value) {
		m_OwningEntity->TriggerEvent(eTriggerEventType::PET_ON_SWITCH, m_OwningEntity);
		GameMessages::SendPlayFXEffect(m_OwningEntity->GetObjectID(), 1513, u"create", "PetOnSwitch", LWOOBJID_EMPTY, 1, 1, true);
	} else {
		m_OwningEntity->TriggerEvent(eTriggerEventType::PET_OFF_SWITCH, m_OwningEntity);
		GameMessages::SendStopFXEffect(m_OwningEntity, true, "PetOnSwitch");
	}

}

bool BouncerComponent::GetPetEnabled() const {
	return m_PetEnabled;
}

bool BouncerComponent::GetPetBouncerEnabled() const {
	return m_PetBouncerEnabled;
}

void BouncerComponent::LookupPetSwitch() {
	const auto& groups = m_OwningEntity->GetGroups();

	for (const auto& group : groups) {
		const auto& entities = EntityManager::Instance()->GetEntitiesInGroup(group);

		for (auto* entity : entities) {
			auto* switchComponent = entity->GetComponent<SwitchComponent>();

			if (switchComponent != nullptr) {
				switchComponent->SetPetBouncer(this);

				m_PetSwitchLoaded = true;
				m_PetEnabled = true;

				EntityManager::Instance()->SerializeEntity(m_OwningEntity);

				Game::logger->Log("BouncerComponent", "Loaded pet bouncer");
			}
		}
	}

	if (!m_PetSwitchLoaded) {
		Game::logger->Log("BouncerComponent", "Failed to load pet bouncer");

		m_OwningEntity->AddCallbackTimer(0.5f, [this]() {
			LookupPetSwitch();
			});
	}
}
