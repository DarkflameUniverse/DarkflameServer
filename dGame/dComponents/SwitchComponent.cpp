#include "SwitchComponent.h"
#include "EntityManager.h"
#include "eTriggerEventType.h"
#include "RenderComponent.h"

std::vector<SwitchComponent*> SwitchComponent::petSwitches;

SwitchComponent::SwitchComponent(Entity* parent) : Component(parent) {
	m_Active = false;

	m_ResetTime = m_ParentEntity->GetVarAs<int32_t>(u"switch_reset_time");

	m_Rebuild = m_ParentEntity->GetComponent<RebuildComponent>();
}

SwitchComponent::~SwitchComponent() {
	const auto& iterator = std::find(petSwitches.begin(), petSwitches.end(), this);

	if (iterator != petSwitches.end()) {
		petSwitches.erase(iterator);
	}
}

void SwitchComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_Active);
}

void SwitchComponent::SetActive(bool active) {
	m_Active = active;

	if (m_PetBouncer != nullptr) {
		m_PetBouncer->SetPetBouncerEnabled(active);
	}
}

bool SwitchComponent::GetActive() const {
	return m_Active;
}

void SwitchComponent::EntityEnter(Entity* entity) {
	if (!m_Active) {
		if (m_Rebuild) {
			if (m_Rebuild->GetState() != eRebuildState::COMPLETED) return;
		}
		m_Active = true;
		if (!m_ParentEntity) return;
		m_ParentEntity->TriggerEvent(eTriggerEventType::ACTIVATED, entity);

		const auto grpName = m_ParentEntity->GetVarAsString(u"grp_name");

		if (!grpName.empty()) {
			const auto entities = EntityManager::Instance()->GetEntitiesInGroup(grpName);

			for (auto* entity : entities) {
				entity->OnFireEventServerSide(entity, "OnActivated");
			}
		}

		m_Timer = m_ResetTime;

		if (m_PetBouncer != nullptr) {
			GameMessages::SendPlayFXEffect(m_ParentEntity->GetObjectID(), 2602, u"pettriggeractive", "BounceEffect", LWOOBJID_EMPTY, 1, 1, true);
			RenderComponent::PlayAnimation(m_ParentEntity, u"engaged");
			m_PetBouncer->SetPetBouncerEnabled(true);
		} else {
			EntityManager::Instance()->SerializeEntity(m_ParentEntity);
		}

	}
}

void SwitchComponent::EntityLeave(Entity* entity) {

}

void SwitchComponent::Update(float deltaTime) {
	if (m_Active) {
		m_Timer -= deltaTime;

		if (m_Timer <= 0.0f) {
			m_Active = false;
			if (!m_ParentEntity) return;
			m_ParentEntity->TriggerEvent(eTriggerEventType::DEACTIVATED, m_ParentEntity);

			const auto grpName = m_ParentEntity->GetVarAsString(u"grp_name");

			if (!grpName.empty()) {
				const auto entities = EntityManager::Instance()->GetEntitiesInGroup(grpName);

				for (auto* entity : entities) {
					entity->OnFireEventServerSide(entity, "OnDectivated");
				}
			}

			if (m_PetBouncer != nullptr) {
				m_PetBouncer->SetPetBouncerEnabled(false);
			} else {
				EntityManager::Instance()->SerializeEntity(m_ParentEntity);
			}
		}
	}
}

Entity* SwitchComponent::GetParentEntity() const {
	return m_ParentEntity;
}

SwitchComponent* SwitchComponent::GetClosestSwitch(NiPoint3 position) {
	float closestDistance = 0;
	SwitchComponent* closest = nullptr;

	for (SwitchComponent* petSwitch : petSwitches) {
		float distance = Vector3::DistanceSquared(petSwitch->m_ParentEntity->GetPosition(), position);

		if (closest == nullptr || distance < closestDistance) {
			closestDistance = distance;
			closest = petSwitch;
		}
	}

	return closest;
}


void SwitchComponent::SetPetBouncer(BouncerComponent* value) {
	m_PetBouncer = value;

	if (value != nullptr) {
		m_PetBouncer->SetPetEnabled(true);
		petSwitches.push_back(this);
	}
}

BouncerComponent* SwitchComponent::GetPetBouncer() const {
	return m_PetBouncer;
}
