#include "CollectibleComponent.h"

#include "MissionComponent.h"
#include "dServer.h"
#include "Amf3.h"
#include "CDClientManager.h"
#include "CDCollectibleComponentTable.h"
#include "CDMissionsTable.h"
#include "eMissionState.h"

CollectibleComponent::CollectibleComponent(Entity* parentEntity, const int32_t componentID, const int32_t collectibleId) :
	Component(parentEntity, componentID), m_CollectibleId(collectibleId) {
	RegisterMsg(&CollectibleComponent::MsgGetObjectReportInfo);
	const auto* const row = CDClientManager::GetTable<CDCollectibleComponentTable>()->GetByID(componentID);
	if (row) m_RequirementMission = row->requirementMission;
}

bool CollectibleComponent::CountsFor(const Entity& player) const {
	if (m_RequirementMission <= 0) return true;
	bool found = false;
	const auto& mission = CDClientManager::GetTable<CDMissionsTable>()->GetByMissionID(m_RequirementMission, found);
	// Not a mission (rows with 66666666), or an achievement: nothing to accept first
	if (!found || !mission.isMission) return true;

	const auto* const missionComponent = player.GetComponent<MissionComponent>();
	if (!missionComponent) return false;
	switch (missionComponent->GetMissionState(m_RequirementMission)) {
	case eMissionState::ACTIVE:
	case eMissionState::READY_TO_COMPLETE:
	case eMissionState::COMPLETE_ACTIVE:
	case eMissionState::COMPLETE_READY_TO_COMPLETE:
		return true;
	default:
		return false;
	}
}

void CollectibleComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	outBitStream.Write(GetCollectibleId());
}

bool CollectibleComponent::MsgGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportMsg) {
	auto& cmptType = reportMsg.info->PushDebug("Collectible");
	auto collectibleID = static_cast<uint32_t>(m_CollectibleId) + static_cast<uint32_t>(Game::server->GetZoneID() << 8);

	cmptType.PushDebug<AMFIntValue>("Component ID") = GetComponentID();

	cmptType.PushDebug<AMFIntValue>("Collectible ID") = GetCollectibleId();
	cmptType.PushDebug<AMFIntValue>("Requirement mission") = m_RequirementMission;
	cmptType.PushDebug<AMFIntValue>("Mission Tracking ID (for save data)") = collectibleID;

	auto* localCharEntity = Game::entityManager->GetEntity(reportMsg.clientID);
	bool collected = false;
	if (localCharEntity) {
		auto* missionComponent = localCharEntity->GetComponent<MissionComponent>();

		if (m_CollectibleId != 0) {
			collected = missionComponent->HasCollectible(collectibleID);
		}
	}

	cmptType.PushDebug<AMFBoolValue>("Has been collected") = collected;
	return true;
}
