#include "NjWuNPC.h"
#include "MissionComponent.h"
#include "Character.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "eMissionState.h"
#include "ePlayerFlag.h"

void NjWuNPC::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {

	// The Dragon statue daily mission
	if (missionID == m_MainDragonMissionID) {
		auto* character = target->GetCharacter();
		auto* missionComponent = target->GetComponent<MissionComponent>();
		if (character == nullptr || missionComponent == nullptr)
			return;

		switch (missionState) {
		case eMissionState::AVAILABLE:
		case eMissionState::COMPLETE_AVAILABLE:
		{
			// Reset the sub missions
			for (const auto& subMissionID : m_SubDragonMissionIDs) {
				missionComponent->RemoveMission(subMissionID);
				missionComponent->AcceptMission(subMissionID);
			}

			character->SetPlayerFlag(ePlayerFlag::NJ_WU_SHOW_DAILY_CHEST, false);

			// Hide the chest
			for (auto* chest : Game::entityManager->GetEntitiesInGroup(m_DragonChestGroup)) {
				GameMessages::SendNotifyClientObject(chest->GetObjectID(), m_ShowChestNotification, 0, -1,
					target->GetObjectID(), "", target->GetSystemAddress());
			}

			return;
		}
		case eMissionState::READY_TO_COMPLETE:
		case eMissionState::COMPLETE_READY_TO_COMPLETE:
		{
			character->SetPlayerFlag(ePlayerFlag::NJ_WU_SHOW_DAILY_CHEST, true);

			// Show the chest
			for (auto* chest : Game::entityManager->GetEntitiesInGroup(m_DragonChestGroup)) {
				GameMessages::SendNotifyClientObject(chest->GetObjectID(), m_ShowChestNotification, 1, -1,
					target->GetObjectID(), "", target->GetSystemAddress());
			}

			auto playerID = target->GetObjectID();
			self->AddCallbackTimer(5.0f, [this, playerID]() {
				auto* player = Game::entityManager->GetEntity(playerID);
				if (player == nullptr)
					return;

				// Stop the dragon effects
				for (auto* dragon : Game::entityManager->GetEntitiesInGroup(m_DragonStatueGroup)) {
					GameMessages::SendStopFXEffect(dragon, true, "on");
				}
				});
		}
		default:
			return;
		}
	}
}
