#include "CollectibleComponent.h"

#include "MissionComponent.h"
#include "dServer.h"
#include "Amf3.h"

CollectibleComponent::CollectibleComponent(Entity* parentEntity, const int32_t componentID, const int32_t collectibleId) :
	Component(parentEntity, componentID), m_CollectibleId(collectibleId) {
	RegisterMsg(&CollectibleComponent::MsgGetObjectReportInfo);
}

void CollectibleComponent::Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {
	outBitStream.Write(GetCollectibleId());
}

bool CollectibleComponent::MsgGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportMsg) {
	auto& cmptType = reportMsg.info->PushDebug("Collectible");
	auto collectibleID = static_cast<uint32_t>(m_CollectibleId) + static_cast<uint32_t>(Game::server->GetZoneID() << 8);

	cmptType.PushDebug<AMFIntValue>("Component ID") = GetComponentID();

	cmptType.PushDebug<AMFIntValue>("Collectible ID") = GetCollectibleId();
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
