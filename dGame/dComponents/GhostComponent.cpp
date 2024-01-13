#include "GhostComponent.h"

// TODO Move ghosting related code from Player to here
GhostComponent::GhostComponent(Entity* parent) : Component(parent) {
	m_GhostReferencePoint = NiPoint3::ZERO;
	m_GhostOverridePoint = NiPoint3::ZERO;
	m_GhostOverride = false;

	m_ObservedEntities.resize(256);
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
	const auto iter = std::find(m_LimboConstructions.begin(), m_LimboConstructions.end(), objectId);
	if (iter == m_LimboConstructions.end()) {
		m_LimboConstructions.push_back(objectId);
	}
}

void GhostComponent::RemoveLimboConstruction(LWOOBJID objectId) {
	const auto iter = std::find(m_LimboConstructions.begin(), m_LimboConstructions.end(), objectId);
	if (iter != m_LimboConstructions.end()) {
		m_LimboConstructions.erase(iter);
	}
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
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == 0 || observedEntity == id) {
			observedEntity = id;

			return;
		}
	}

	m_ObservedEntities.reserve(m_ObservedEntities.size() + 1);

	m_ObservedEntities.push_back(id);
}

bool GhostComponent::IsObserved(int32_t id) {
	return std::find(m_ObservedEntities.begin(), m_ObservedEntities.end(), id) != m_ObservedEntities.end();
}

void GhostComponent::GhostEntity(int32_t id) {
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == id) {
			observedEntity = 0;
		}
	}
}
