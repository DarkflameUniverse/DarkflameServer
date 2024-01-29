#include "GhostComponent.h"

GhostComponent::GhostComponent(Entity* parent) : Component(parent) {
	m_GhostReferencePoint = NiPoint3Constant::ZERO;
	m_GhostOverridePoint = NiPoint3Constant::ZERO;
	m_GhostOverride = false;
}

GhostComponent::~GhostComponent() {
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == 0) continue;

		auto* entity = Game::entityManager->GetGhostCandidate(observedEntity);
		if (!entity) continue;

		entity->SetObservers(entity->GetObservers() - 1);
	}
}

void GhostComponent::SetGhostReferencePoint(const NiPoint3& value) {
	m_GhostReferencePoint = value;
}

void GhostComponent::SetGhostOverridePoint(const NiPoint3& value) {
	m_GhostOverridePoint = value;
}

void GhostComponent::AddLimboConstruction(LWOOBJID objectId) {
	m_LimboConstructions.insert(objectId);
}

void GhostComponent::RemoveLimboConstruction(LWOOBJID objectId) {
	m_LimboConstructions.erase(objectId);
}

void GhostComponent::ConstructLimboEntities() {
	for (const auto& objectId : m_LimboConstructions) {
		auto* entity = Game::entityManager->GetEntity(objectId);
		if (!entity) continue;

		Game::entityManager->ConstructEntity(entity, m_Parent->GetSystemAddress());
	}

	m_LimboConstructions.clear();
}

void GhostComponent::ObserveEntity(int32_t id) {
	m_ObservedEntities.insert(id);
}

bool GhostComponent::IsObserved(int32_t id) {
	return m_ObservedEntities.contains(id);
}

void GhostComponent::GhostEntity(int32_t id) {
	m_ObservedEntities.erase(id);
}
