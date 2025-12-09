#include "GhostComponent.h"
#include "PlayerManager.h"
#include "Character.h"
#include "ControllablePhysicsComponent.h"
#include "UserManager.h"
#include "User.h"

#include "Amf3.h"
#include "GameMessages.h"

GhostComponent::GhostComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_GhostReferencePoint = NiPoint3Constant::ZERO;
	m_GhostOverridePoint = NiPoint3Constant::ZERO;
	m_GhostOverride = false;

	RegisterMsg<GameMessages::ToggleGMInvis>(this, &GhostComponent::OnToggleGMInvis);
	RegisterMsg<GameMessages::GetGMInvis>(this, &GhostComponent::OnGetGMInvis);
	RegisterMsg<GameMessages::GetObjectReportInfo>(this, &GhostComponent::MsgGetObjectReportInfo);
}

GhostComponent::~GhostComponent() {
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == LWOOBJID_EMPTY) continue;

		auto* entity = Game::entityManager->GetGhostCandidate(observedEntity);
		if (!entity) continue;

		entity->SetObservers(entity->GetObservers() - 1);
	}
}

void GhostComponent::LoadFromXml(const tinyxml2::XMLDocument& doc) {
	auto* objElement = doc.FirstChildElement("obj");
	if (!objElement) return;
	auto* ghstElement = objElement->FirstChildElement("ghst");
	if (!ghstElement) return;
	m_IsGMInvisible = ghstElement->BoolAttribute("i");
}

void GhostComponent::UpdateXml(tinyxml2::XMLDocument& doc) {
	auto* objElement = doc.FirstChildElement("obj");
	if (!objElement) return;
	auto* ghstElement = objElement->FirstChildElement("ghst");
	if (ghstElement) objElement->DeleteChild(ghstElement);
	// Only save if GM invisible
	const auto* const user = UserManager::Instance()->GetUser(m_Parent->GetSystemAddress());
	if (!m_IsGMInvisible || !user || user->GetMaxGMLevel() < eGameMasterLevel::FORUM_MODERATOR) return;
	ghstElement = objElement->InsertNewChildElement("ghst");
	if (ghstElement) ghstElement->SetAttribute("i", m_IsGMInvisible);
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

void GhostComponent::ObserveEntity(LWOOBJID id) {
	m_ObservedEntities.insert(id);
}

bool GhostComponent::IsObserved(LWOOBJID id) {
	return m_ObservedEntities.contains(id);
}

void GhostComponent::GhostEntity(LWOOBJID id) {
	m_ObservedEntities.erase(id);
}

bool GhostComponent::OnToggleGMInvis(GameMessages::GameMsg& msg) {
	// TODO: disabled for now while bugs are fixed
	return false;
	auto& gmInvisMsg = static_cast<GameMessages::ToggleGMInvis&>(msg);
	gmInvisMsg.bStateOut = !m_IsGMInvisible;
	m_IsGMInvisible = !m_IsGMInvisible;
	LOG_DEBUG("GM Invisibility toggled to: %s", m_IsGMInvisible ? "true" : "false");
	gmInvisMsg.Send(UNASSIGNED_SYSTEM_ADDRESS);
	auto* thisUser = UserManager::Instance()->GetUser(m_Parent->GetSystemAddress());
	for (const auto& player : PlayerManager::GetAllPlayers()) {
		if (!player || player->GetObjectID() == m_Parent->GetObjectID()) continue;
		auto* toUser = UserManager::Instance()->GetUser(player->GetSystemAddress());
		if (m_IsGMInvisible) {
			if (toUser->GetMaxGMLevel() < thisUser->GetMaxGMLevel()) {
				Game::entityManager->DestructEntity(m_Parent, player->GetSystemAddress());
			}
		} else {
			if (toUser->GetMaxGMLevel() >= thisUser->GetMaxGMLevel()) {
				Game::entityManager->ConstructEntity(m_Parent, player->GetSystemAddress());
				auto* controllableComp = m_Parent->GetComponent<ControllablePhysicsComponent>();
				controllableComp->SetDirtyPosition(true);
			}
		}
	}
	Game::entityManager->SerializeEntity(m_Parent);

	return true;
}

bool GhostComponent::OnGetGMInvis(GameMessages::GameMsg& msg) {
	LOG_DEBUG("GM Invisibility requested: %s", m_IsGMInvisible ? "true" : "false");
	auto& gmInvisMsg = static_cast<GameMessages::GetGMInvis&>(msg);
	// TODO: disabled for now while bugs are fixed
	// gmInvisMsg.bGMInvis = m_IsGMInvisible;
	// return gmInvisMsg.bGMInvis;
	gmInvisMsg.bGMInvis = false;
	return false;
}

bool GhostComponent::MsgGetObjectReportInfo(GameMessages::GameMsg& msg) {
	auto& reportMsg = static_cast<GameMessages::GetObjectReportInfo&>(msg);
	auto& cmptType = reportMsg.info->PushDebug("Ghost");
	cmptType.PushDebug<AMFIntValue>("Component ID") = GetComponentID();
	cmptType.PushDebug<AMFBoolValue>("Is GM Invis") = false;

	return true;
}
