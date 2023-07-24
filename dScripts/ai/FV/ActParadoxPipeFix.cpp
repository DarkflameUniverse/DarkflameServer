#include "ActParadoxPipeFix.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "eEndBehavior.h"

void ActParadoxPipeFix::OnRebuildComplete(Entity* self, Entity* target) {
	const auto myGroup = "AllPipes";

	const auto groupObjs = Game::entityManager->GetEntitiesInGroup(myGroup);

	auto indexCount = 0;

	self->SetVar(u"PlayerID", target->GetObjectID());

	for (auto* object : groupObjs) {
		if (object == self) {
			continue;
		}

		auto* rebuildComponent = object->GetComponent<RebuildComponent>();

		if (rebuildComponent->GetState() == eRebuildState::COMPLETED) {
			indexCount++;
		}
	}

	if (indexCount >= 2) {
		const auto refinery = Game::entityManager->GetEntitiesInGroup("Paradox");

		if (!refinery.empty()) {
			GameMessages::SendPlayFXEffect(refinery[0]->GetObjectID(), 3999, u"create", "pipeFX");
		}

		for (auto* object : groupObjs) {
			auto* player = Game::entityManager->GetEntity(object->GetVar<LWOOBJID>(u"PlayerID"));

			if (player != nullptr) {
				auto* missionComponent = player->GetComponent<MissionComponent>();

				if (missionComponent != nullptr) {
					missionComponent->ForceProgressTaskType(769, 1, 1, false);
				}

				GameMessages::SendPlayCinematic(player->GetObjectID(), u"ParadoxPipeFinish", player->GetSystemAddress(), true, true, false, false, eEndBehavior::RETURN, false, 2.0f);
			}

			object->SetVar(u"PlayerID", LWOOBJID_EMPTY);
		}
	}
}

void ActParadoxPipeFix::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::RESETTING) {
		const auto refinery = Game::entityManager->GetEntitiesInGroup("Paradox");

		if (!refinery.empty()) {
			GameMessages::SendStopFXEffect(refinery[0], true, "pipeFX");
		}
	}
}
