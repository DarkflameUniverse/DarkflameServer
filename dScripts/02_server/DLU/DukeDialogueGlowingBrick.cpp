#include "DukeDialogueGlowingBrick.h"

#include "eMissionState.h"

#include "Recorder.h"
#include "MissionComponent.h"
#include "InventoryComponent.h"

void DukeDialogueGlowingBrick::OnStartup(Entity* self) {
	Game::logger->Log("DukeDialogueGlowingBrick", "OnStartup");
}

void DukeDialogueGlowingBrick::OnTimerDone(Entity* self, std::string timerName) {
}

void DukeDialogueGlowingBrick::OnMissionDialogueOK(Entity* self, Entity* target, int missionID, eMissionState missionState) {
	if (missionID != 201453) {
		return;
	}

	if (missionState != eMissionState::AVAILABLE) {
		return;
	}

	auto* recorder = Recording::Recorder::LoadFromFile("DukeGlowing.xml");

	if (recorder == nullptr) {
		return;
	}

	auto* actor = recorder->ActFor(self, target);

	if (actor == nullptr) {
		return;
	}

	const auto targetID = target->GetObjectID();
	const auto actorID = actor->GetObjectID();

	self->AddCallbackTimer(3.0f, [targetID] () {
		auto* target = Game::entityManager->GetEntity(targetID);
	
		if (target == nullptr) {
			return;
		}

		auto* missionComponent = target->GetComponent<MissionComponent>();

		if (missionComponent == nullptr) {
			return;
		}

		missionComponent->CompleteMission(201453);
	});

	self->AddCallbackTimer(recorder->GetDuration() + 10.0f, [recorder, self, actorID, targetID] () {
		auto* target = Game::entityManager->GetEntity(targetID);
		auto* actor = Game::entityManager->GetEntity(actorID);

		if (target == nullptr || actor == nullptr) {
			return;
		}

		recorder->StopActingFor(actor, self, targetID);

		delete recorder;
	});
}

void DukeDialogueGlowingBrick::OnRespondToMission(Entity* self, int missionID, Entity* player, int reward) {

}
