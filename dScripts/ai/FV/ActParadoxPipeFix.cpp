#include "ActParadoxPipeFix.h"
#include "EntityManager.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "MissionComponent.h"
#include "eEndBehavior.h"

void ActParadoxPipeFix::OnQuickBuildComplete(Entity* self, Entity* target) {
	const auto myGroup = "AllPipes";

	const auto groupObjs = Game::entityManager->GetEntitiesInGroup(myGroup);

	auto indexCount = 0;

	self->SetVar(u"PlayerID", target->GetObjectID());

	for (auto* object : groupObjs) {
		if (object == self) {
			continue;
		}

		auto* quickBuildComponent = object->GetComponent<QuickBuildComponent>();

		if (quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
			indexCount++;
		}
	}

	if (indexCount >= 2) {
		const auto refinery = Game::entityManager->GetEntitiesInGroup("Paradox");

		if (!refinery.empty()) {
			GameMessages::PlayFXEffect(refinery[0]->GetObjectID(), 3999, u"create", "pipeFX").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		for (auto* object : groupObjs) {
			auto* player = Game::entityManager->GetEntity(object->GetVar<LWOOBJID>(u"PlayerID"));

			if (player != nullptr) {
				auto* missionComponent = player->GetComponent<MissionComponent>();

				if (missionComponent != nullptr) {
					missionComponent->ForceProgressTaskType(769, 1, 1, false);
				}

				GameMessages::PlayCinematic cinematic;
				cinematic.target = player->GetObjectID();
				cinematic.pathName = u"ParadoxPipeFinish";
				cinematic.allowGhostUpdates = true;
				cinematic.bCloseMultiInteract = true;
				cinematic.bSendServerNotify = false;
				cinematic.bUseControlledObjectForAudioListener = false;
				cinematic.endBehavior = eEndBehavior::RETURN;
				cinematic.hidePlayerDuringCine = false;
				cinematic.leadIn = 2.0f;
				cinematic.Send(player->GetSystemAddress());
			}

			object->SetVar(u"PlayerID", LWOOBJID_EMPTY);
		}
	}
}

void ActParadoxPipeFix::OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) {
	if (state == eQuickBuildState::RESETTING) {
		const auto refinery = Game::entityManager->GetEntitiesInGroup("Paradox");

		if (!refinery.empty()) {
			GameMessages::StopFXEffect(refinery[0]->GetObjectID(), true, "pipeFX").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
	}
}
