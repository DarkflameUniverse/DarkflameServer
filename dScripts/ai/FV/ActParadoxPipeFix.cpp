#include "ActParadoxPipeFix.h"
#include "EntityManager.h"
#include "QuickBuildComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "eEndBehavior.h"

void ActParadoxPipeFix::OnRebuildComplete(Entity* self, Entity* target) {
	const auto myGroup = "AllPipes";

	const auto groupObjs = EntityManager::Instance()->GetEntitiesInGroup(myGroup);

	auto indexCount = 0;

	self->SetVar(u"PlayerID", target->GetObjectID());

	for (auto* object : groupObjs) {
		if (object == self) {
			continue;
		}

		auto* quickBuildComponent = object->GetComponent<QuickBuildComponent>();

		if (quickBuildComponent->GetState() == eRebuildState::COMPLETED) {
			indexCount++;
		}
	}

	if (indexCount >= 2) {
		const auto refinery = EntityManager::Instance()->GetEntitiesInGroup("Paradox");

		if (!refinery.empty()) {
			GameMessages::SendPlayFXEffect(refinery[0]->GetObjectID(), 3999, u"create", "pipeFX");
		}

		for (auto* object : groupObjs) {
			auto* player = EntityManager::Instance()->GetEntity(object->GetVar<LWOOBJID>(u"PlayerID"));

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
		const auto refinery = EntityManager::Instance()->GetEntitiesInGroup("Paradox");

		if (!refinery.empty()) {
			GameMessages::SendStopFXEffect(refinery[0], true, "pipeFX");
		}
	}
}
