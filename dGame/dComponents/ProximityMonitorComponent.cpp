#include "ProximityMonitorComponent.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"
#include "Amf3.h"
#include "dpShapeSphere.h"

const std::unordered_set<LWOOBJID> ProximityMonitorComponent::m_EmptyObjectSet = {};

ProximityMonitorComponent::ProximityMonitorComponent(Entity* parent, const int32_t componentID, int radiusSmall, int radiusLarge) : Component(parent, componentID) {
	if (radiusSmall != -1 && radiusLarge != -1) {
		SetProximityRadius(radiusSmall, "rocketSmall");
		SetProximityRadius(radiusLarge, "rocketLarge");
	}
	RegisterMsg(&ProximityMonitorComponent::OnGetObjectReportInfo);
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

const std::unordered_set<LWOOBJID>& ProximityMonitorComponent::GetProximityObjects(const std::string& name) const {
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

bool ProximityMonitorComponent::OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo) {
	auto& proxInfo = reportInfo.info->PushDebug("Proximity Monitor");
	for (const auto& [name, entity] : m_ProximitiesData) {
		if (!entity) continue;
		auto& proxAmf = proxInfo.PushDebug(name);
		const auto* const shape = entity->GetShape();
		if (shape && shape->GetShapeType() == dpShapeType::Sphere) {
			const auto* const sphere = static_cast<const dpShapeSphere*>(shape);
			proxAmf.PushDebug<AMFDoubleValue>("Radius") = sphere->GetRadius();
		}
		proxAmf.PushDebug<AMFBoolValue>("Sleeping") = entity->GetSleeping();
		proxAmf.PushDebug<AMFDoubleValue>("Scale") = entity->GetScale();
		proxAmf.PushDebug<AMFBoolValue>("Gargantuan") = entity->GetIsGargantuan();
		proxAmf.PushDebug<AMFBoolValue>("Static") = entity->GetIsStatic();
		proxAmf.PushDebug("Position").PushDebug(entity->GetPosition());
		proxAmf.PushDebug("Rotation").PushDebug(entity->GetRotation());
		auto& collidingAmf = proxAmf.PushDebug("Colliding Objects");
		int i = 1;
		for (const auto& colliding : entity->GetCurrentlyCollidingObjects()) {
			collidingAmf.PushDebug<AMFStringValue>(std::to_string(i++), "LWOOBJID") = std::to_string(colliding);
		}
	}

	return true;
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

