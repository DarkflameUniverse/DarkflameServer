#include "VeEpsilonServer.h"

#include "EntityManager.h"
#include "GameMessages.h"
#include "eMissionState.h"
#include "Entity.h"

void VeEpsilonServer::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	// Resets the player flags that track which consoles they've used
	if ((missionID == m_ConsoleMissionID || missionID == m_ConsoleRepeatMissionID)
		&& (missionState == eMissionState::AVAILABLE || missionState == eMissionState::COMPLETE_AVAILABLE)) {

		GameMessages::SetFlag setFlag{};
		setFlag.target = target->GetObjectID();
		for (auto i = 0; i < 10; i++) {
			setFlag.iFlagId = m_ConsoleBaseFlag + i;
			setFlag.bFlag = false;
			SEND_ENTITY_MSG(setFlag);
		}
	}

	// Notify the client that all objects have updated
	self->AddCallbackTimer(3.0f, [this]() {
		for (const auto* console : Game::entityManager->GetEntitiesInGroup(m_ConsoleGroup)) {
			GameMessages::SendNotifyClientObject(console->GetObjectID(), u"");
		}
		});
}
