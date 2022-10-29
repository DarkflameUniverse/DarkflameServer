#include "BouncerComponent.h"

#include "EntityManager.h"
#include "dZoneManager.h"
#include "SwitchComponent.h"
#include "Game.h"
#include "dLogger.h"
#include "GameMessages.h"
#include <BitStream.h>

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
	return m_Parent;
}

void BouncerComponent::SetPetEnabled(bool value) {
	m_PetEnabled = value;

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void BouncerComponent::SetPetBouncerEnabled(bool value) {
	m_PetBouncerEnabled = value;

	GameMessages::SendBouncerActiveStatus(m_Parent->GetObjectID(), value, UNASSIGNED_SYSTEM_ADDRESS);

	EntityManager::Instance()->SerializeEntity(m_Parent);

	if (value) {
		GameMessages::SendPlayFXEffect(m_Parent->GetObjectID(), 1513, u"create", "PetOnSwitch", LWOOBJID_EMPTY, 1, 1, true);
	} else {
		GameMessages::SendStopFXEffect(m_Parent, true, "PetOnSwitch");
	}

}

bool BouncerComponent::GetPetEnabled() const {
	return m_PetEnabled;
}

bool BouncerComponent::GetPetBouncerEnabled() const {
	return m_PetBouncerEnabled;
}

void BouncerComponent::LookupPetSwitch() {
	const auto& groups = m_Parent->GetGroups();

	for (const auto& group : groups) {
		const auto& entities = EntityManager::Instance()->GetEntitiesInGroup(group);

		for (auto* entity : entities) {
			auto* switchComponent = entity->GetComponent<SwitchComponent>();

			if (switchComponent != nullptr) {
				switchComponent->SetPetBouncer(this);

				m_PetSwitchLoaded = true;
				m_PetEnabled = true;

				EntityManager::Instance()->SerializeEntity(m_Parent);

				Game::logger->Log("BouncerComponent", "Loaded pet bouncer");
			}
		}
	}

	if (!m_PetSwitchLoaded) {
		Game::logger->Log("BouncerComponent", "Failed to load pet bouncer");

		m_Parent->AddCallbackTimer(0.5f, [this]() {
			LookupPetSwitch();
			});
	}
}
