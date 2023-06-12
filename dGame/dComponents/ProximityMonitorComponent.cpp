#include "ProximityMonitorComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"
#include "CDClientManager.h"
#include "CDProximityMonitorComponentTable.h"

const std::map<LWOOBJID, dpEntity*> ProximityMonitorComponent::m_EmptyObjectMap = {};

ProximityMonitorComponent::ProximityMonitorComponent(Entity* parent, int32_t componentId) : Component(parent) {
	m_ComponentId = componentId;
}

ProximityMonitorComponent::~ProximityMonitorComponent() {
	for (const auto& en : m_ProximitiesData) {
		if (!en.second) continue;

		dpWorld::Instance().RemoveEntity(en.second);
	}

	m_ProximitiesData.clear();
}

void ProximityMonitorComponent::LoadTemplateData() {
	if (m_ComponentId == -1) return;
	auto* proxCompTable = CDClientManager::Instance().GetTable<CDProximityMonitorComponentTable>();
	auto proxCompData = proxCompTable->Query([this](CDProximityMonitorComponent entry) { return (entry.id == this->m_ComponentId); });

	if (!proxCompData.empty()) {
		float radiusSmall = -1.0f;
		float radiusLarge = -1.0f;
		auto proximitySplit = GeneralUtils::SplitString(proxCompData[0].Proximities, ',');
		if (proximitySplit.size() < 2) return;
		GeneralUtils::TryParse(proximitySplit.at(0), radiusSmall);
		GeneralUtils::TryParse(proximitySplit.at(1), radiusLarge);
		if (radiusSmall != -1.0f && radiusLarge != -1.0f) {
			SetProximityRadius(radiusSmall, "rocketSmall");
			SetProximityRadius(radiusLarge, "rocketLarge");
		}
	}
}

void ProximityMonitorComponent::SetProximityRadius(float proxRadius, const std::string& name) {
	dpEntity* en = new dpEntity(m_ParentEntity->GetObjectID(), proxRadius);
	en->SetPosition(m_ParentEntity->GetPosition());

	dpWorld::Instance().AddEntity(en);
	m_ProximitiesData.insert(std::make_pair(name, en));
}

void ProximityMonitorComponent::SetProximityRadius(dpEntity* entity, const std::string& name) {
	dpWorld::Instance().AddEntity(entity);
	entity->SetPosition(m_ParentEntity->GetPosition());
	m_ProximitiesData.insert(std::make_pair(name, entity));
}

const std::map<LWOOBJID, dpEntity*>& ProximityMonitorComponent::GetProximityObjects(const std::string& name) {
	const auto& iter = m_ProximitiesData.find(name);

	if (iter == m_ProximitiesData.end()) {
		return m_EmptyObjectMap;
	}

	return iter->second->GetCurrentlyCollidingObjects();
}

bool ProximityMonitorComponent::IsInProximity(const std::string& name, LWOOBJID objectID) {
	const auto& iter = m_ProximitiesData.find(name);

	if (iter == m_ProximitiesData.end()) {
		return false;
	}

	const auto& collitions = iter->second->GetCurrentlyCollidingObjects();

	return collitions.find(objectID) != collitions.end();
}

void ProximityMonitorComponent::Update(float deltaTime) {
	for (const auto& prox : m_ProximitiesData) {
		if (!prox.second) continue;

		//Process enter events
		for (auto* en : prox.second->GetNewObjects()) {
			m_ParentEntity->OnCollisionProximity(en->GetObjectID(), prox.first, "ENTER");
		}

		//Process exit events
		for (auto* en : prox.second->GetRemovedObjects()) {
			m_ParentEntity->OnCollisionProximity(en->GetObjectID(), prox.first, "LEAVE");
		}
	}
}

