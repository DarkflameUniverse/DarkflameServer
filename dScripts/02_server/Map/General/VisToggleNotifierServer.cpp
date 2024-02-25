#include "VisToggleNotifierServer.h"
#include "eMissionState.h"
#include "Game.h"
#include "dZoneManager.h"

void VisToggleNotifierServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionId, eMissionState missionState) {
	auto itr = m_GameVariables.find(missionId);
	if (itr != m_GameVariables.end()) {
		bool visible = true;
		if (missionState == eMissionState::READY_TO_COMPLETE || missionState == eMissionState::COMPLETE_READY_TO_COMPLETE) {
			visible = false;
		}

		auto spawners = Game::zoneManager->GetSpawnersByName(itr->second);
		if (spawners.empty()) return;
		for (const auto spawner : spawners) {
			auto spawnedObjIds = spawner->GetSpawnedObjectIDs();
			for (const auto& objId : spawnedObjIds) {
				GameMessages::SendNotifyClientObject(objId, u"SetVisibility", visible);
			}
		}
	}
}
