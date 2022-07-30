#include "VeEpsilonServer.h"
#include "Character.h"
#include "EntityManager.h"
#include "GameMessages.h"

void VeEpsilonServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, MissionState missionState) {
	auto* character = target->GetCharacter();
	if (character == nullptr)
		return;

	// Resets the player flags that track which consoles they've used
	if ((missionID == m_ConsoleMissionID || missionID == m_ConsoleRepeatMissionID)
		&& (missionState == MissionState::MISSION_STATE_AVAILABLE || missionState == MissionState::MISSION_STATE_COMPLETE_AVAILABLE)) {

		for (auto i = 0; i < 10; i++) {
			character->SetPlayerFlag(m_ConsoleBaseFlag + i, false);
		}
	}

	// Notify the client that all objects have updated
	self->AddCallbackTimer(3.0f, [this]() {
		for (const auto* console : EntityManager::Instance()->GetEntitiesInGroup(m_ConsoleGroup)) {
			GameMessages::SendNotifyClientObject(console->GetObjectID(), u"");
		}
		});
}
