#include "SwitchComponent.h"
#include "EntityManager.h"
#include "eTriggerEventType.h"
#include "RenderComponent.h"

std::vector<SwitchComponent*> SwitchComponent::petSwitches;

SwitchComponent::SwitchComponent(const LWOOBJID& parentEntityId) : Component{ parentEntityId } {
	m_Active = false;

	auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);

	m_ResetTime = parentEntity->GetVarAs<int32_t>(u"switch_reset_time");

	m_QuickBuild = parentEntity->GetComponent<QuickBuildComponent>();
}

SwitchComponent::~SwitchComponent() {
	const auto& iterator = std::find(petSwitches.begin(), petSwitches.end(), this);

	if (iterator != petSwitches.end()) {
		petSwitches.erase(iterator);
	}
}

void SwitchComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
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
		if (m_QuickBuild) {
			if (m_QuickBuild->GetState() != eQuickBuildState::COMPLETED) return;
		}
		m_Active = true;

		auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);

		parentEntity->TriggerEvent(eTriggerEventType::ACTIVATED, entity);

		const auto grpName = parentEntity->GetVarAsString(u"grp_name");

		if (!grpName.empty()) {
			const auto entities = Game::entityManager->GetEntitiesInGroup(grpName);

			for (auto* entity : entities) {
				entity->OnFireEventServerSide(entity, "OnActivated");
			}
		}

		m_Timer = m_ResetTime;

		if (m_PetBouncer != nullptr) {
			GameMessages::SendPlayFXEffect(m_Parent, 2602, u"pettriggeractive", "BounceEffect", LWOOBJID_EMPTY, 1, 1, true);
			RenderComponent::PlayAnimation(parentEntity, u"engaged");
			m_PetBouncer->SetPetBouncerEnabled(true);
		} else {
			Game::entityManager->SerializeEntity(m_Parent);
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

			auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);

			parentEntity->TriggerEvent(eTriggerEventType::DEACTIVATED, Game::entityManager->GetEntity(m_Parent));

			const auto grpName = parentEntity->GetVarAsString(u"grp_name");

			if (!grpName.empty()) {
				const auto entities = Game::entityManager->GetEntitiesInGroup(grpName);

				for (auto* entity : entities) {
					entity->OnFireEventServerSide(entity, "OnDectivated");
				}
			}

			if (m_PetBouncer) {
				m_PetBouncer->SetPetBouncerEnabled(false);
			} else {
				Game::entityManager->SerializeEntity(m_Parent);
			}
		}
	}
}

Entity* SwitchComponent::GetParentEntity() const {
	return Game::entityManager->GetEntity(m_Parent);
}

SwitchComponent* SwitchComponent::GetClosestSwitch(NiPoint3 position) {
	float closestDistance = 0;
	SwitchComponent* closest = nullptr;

	for (SwitchComponent* petSwitch : petSwitches) {
		float distance = Vector3::DistanceSquared(petSwitch->GetParent()->GetPosition(), position);

		if (!closest || distance < closestDistance) {
			closestDistance = distance;
			closest = petSwitch;
		}
	}

	return closest;
}


void SwitchComponent::SetPetBouncer(BouncerComponent* value) {
	m_PetBouncer = value;

	if (value) {
		m_PetBouncer->SetPetEnabled(true);
		petSwitches.push_back(this);
	}
}

BouncerComponent* SwitchComponent::GetPetBouncer() const {
	return m_PetBouncer;
}
