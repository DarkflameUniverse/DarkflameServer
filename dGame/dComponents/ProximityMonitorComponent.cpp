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
			AddProximityRadius(radiusSmall, "rocketSmall");
			AddProximityRadius(radiusLarge, "rocketLarge");
		}
	}
}

void ProximityMonitorComponent::AddProximityRadius(float proxRadius, const std::string& name) {
	dpEntity* entity = new dpEntity(m_ParentEntity->GetObjectID(), proxRadius);
	AddProximityRadius(entity, name);
}

void ProximityMonitorComponent::AddProximityRadius(const BoxDimensions& dimensions, const std::string& name) {
	dpEntity* entity = new dpEntity(m_ParentEntity->GetObjectID(), dimensions);
	AddProximityRadius(entity, name);
}

void ProximityMonitorComponent::AddProximityRadius(dpEntity* entity, const std::string& name) {
	entity->SetPosition(m_ParentEntity->GetPosition());

	dpWorld::Instance().AddEntity(entity);
	auto existing = m_ProximitiesData.find(name);
	if (existing != m_ProximitiesData.end()) {
		dpWorld::Instance().RemoveEntity(existing->second);
	}
	m_ProximitiesData.insert_or_assign(name, entity);
}

const std::map<LWOOBJID, dpEntity*>& ProximityMonitorComponent::GetProximityObjects(const std::string& name) {
	const auto& iter = m_ProximitiesData.find(name);

	return iter == m_ProximitiesData.end() ? m_EmptyObjectMap : iter->second->GetCurrentlyCollidingObjects();
}

bool ProximityMonitorComponent::IsInProximity(const std::string& name, LWOOBJID objectID) {
	const auto& iter = m_ProximitiesData.find(name);

	if (iter == m_ProximitiesData.end()) {
		return false;
	}

	const auto& collisions = iter->second->GetCurrentlyCollidingObjects();

	return collisions.find(objectID) != collisions.end();
}

void ProximityMonitorComponent::Update(float deltaTime) {
	for (const auto& [proximityName, proximityEntity] : m_ProximitiesData) {
		if (!proximityEntity) continue;

		//Process enter events
		for (auto* en : proximityEntity->GetNewObjects()) {
			m_ParentEntity->OnCollisionProximity(en->GetObjectID(), proximityName, "ENTER");
		}

		//Process exit events
		for (auto* en : proximityEntity->GetRemovedObjects()) {
			m_ParentEntity->OnCollisionProximity(en->GetObjectID(), proximityName, "LEAVE");
		}
	}
}
