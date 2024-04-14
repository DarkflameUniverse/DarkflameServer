#include "ProximityMonitorComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"

const std::unordered_set<LWOOBJID> ProximityMonitorComponent::m_EmptyObjectSet = {};

ProximityMonitorComponent::ProximityMonitorComponent(Entity* parent, int radiusSmall, int radiusLarge) : Component(parent) {
	if (radiusSmall != -1 && radiusLarge != -1) {
		SetProximityRadius(radiusSmall, "rocketSmall");
		SetProximityRadius(radiusLarge, "rocketLarge");
	}
}

ProximityMonitorComponent::~ProximityMonitorComponent() {
	for (const auto& en : m_ProximitiesData) {
		if (!en.second) continue;

		dpWorld::RemoveEntity(en.second);
	}

	m_ProximitiesData.clear();
}

void ProximityMonitorComponent::SetProximityRadius(float proxRadius, const std::string& name) {
	dpEntity* en = new dpEntity(m_Parent->GetObjectID(), proxRadius);
	en->SetPosition(m_Parent->GetPosition());

	dpWorld::AddEntity(en);
	m_ProximitiesData.insert(std::make_pair(name, en));
}

void ProximityMonitorComponent::SetProximityRadius(dpEntity* entity, const std::string& name) {
	dpWorld::AddEntity(entity);
	entity->SetPosition(m_Parent->GetPosition());
	m_ProximitiesData.insert(std::make_pair(name, entity));
}

const std::unordered_set<LWOOBJID>& ProximityMonitorComponent::GetProximityObjects(const std::string& name) {
	const auto iter = m_ProximitiesData.find(name);

	if (iter == m_ProximitiesData.cend()) {
		return m_EmptyObjectSet;
	}

	return iter->second->GetCurrentlyCollidingObjects();
}

bool ProximityMonitorComponent::IsInProximity(const std::string& name, LWOOBJID objectID) {
	const auto iter = m_ProximitiesData.find(name);

	if (iter == m_ProximitiesData.cend()) {
		return false;
	}

	const auto& collisions = iter->second->GetCurrentlyCollidingObjects();

	return collisions.contains(objectID);
}

void ProximityMonitorComponent::Update(float deltaTime) {
	for (const auto& prox : m_ProximitiesData) {
		if (!prox.second) continue;

		prox.second->SetPosition(m_Parent->GetPosition());
		//Process enter events
		for (const auto id : prox.second->GetNewObjects()) {
			m_Parent->OnCollisionProximity(id, prox.first, "ENTER");
		}

		//Process exit events
		for (const auto id : prox.second->GetRemovedObjects()) {
			m_Parent->OnCollisionProximity(id, prox.first, "LEAVE");
		}
	}
}

