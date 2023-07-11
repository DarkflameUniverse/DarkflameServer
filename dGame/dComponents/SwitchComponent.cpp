#include "SwitchComponent.h"
#include "EntityManager.h"
#include "eTriggerEventType.h"
#include "RenderComponent.h"

std::vector<SwitchComponent*> SwitchComponent::switches;

SwitchComponent::SwitchComponent(Entity* parent) : Component(parent) {
	m_Active = false;
}

void SwitchComponent::Startup() {
	m_Rebuild = m_ParentEntity->GetComponent<QuickBuildComponent>();
}

void SwitchComponent::LoadConfigData() {
	m_ResetTime = m_ParentEntity->GetVarAs<int32_t>(u"switch_reset_time");
}

SwitchComponent::~SwitchComponent() {
	switches.erase(std::remove(switches.begin(), switches.end(), this), switches.end());
}

void SwitchComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write(m_Active);
}

void SwitchComponent::SetActive(const bool active) {
	if (m_Active == active) return;
	m_Active = active;
	if (m_Bouncer) m_Bouncer->SetBouncerEnabled(active);
}

void SwitchComponent::EntityEnter(Entity* entity) {
	if (m_Active) return;
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

	if (m_Bouncer) {
		GameMessages::SendPlayFXEffect(m_ParentEntity->GetObjectID(), 2602, u"pettriggeractive", "BounceEffect", LWOOBJID_EMPTY, 1, 1, true);
		RenderComponent::PlayAnimation(m_ParentEntity, u"engaged");
		m_Bouncer->SetBouncerEnabled(true);
	} else {
		EntityManager::Instance()->SerializeEntity(m_ParentEntity);
	}
}

void SwitchComponent::Update(const float deltaTime) {
	if (!m_Active) return;
	m_Timer -= deltaTime;

	if (m_Timer > 0.0f) return;

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

	if (m_Bouncer) {
		m_Bouncer->SetBouncerEnabled(false);
	} else {
		EntityManager::Instance()->SerializeEntity(m_ParentEntity);
	}
}

SwitchComponent* SwitchComponent::GetClosestSwitch(const NiPoint3& position) {
	float closestDistance = 0.0f;
	SwitchComponent* closest = nullptr;

	for (auto* petSwitch : switches) {
		float distance = Vector3::DistanceSquared(petSwitch->m_ParentEntity->GetPosition(), position);

		if (closest && distance >= closestDistance) continue;
		closestDistance = distance;
		closest = petSwitch;
	}

	return closest;
}


void SwitchComponent::SetPetBouncer(BouncerComponent* value) {
	m_Bouncer = value;

	if (!value) return;
	m_Bouncer->SetBounceOnCollision(true);
	EntityManager::Instance()->SerializeEntity(m_Bouncer->GetParentEntity());
	switches.push_back(this);
}
