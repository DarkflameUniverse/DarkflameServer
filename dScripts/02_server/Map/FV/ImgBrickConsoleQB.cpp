#include "ImgBrickConsoleQB.h"
#include "RebuildComponent.h"
#include "dZoneManager.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "eMissionState.h"
#include "InventoryComponent.h"
#include "eTerminateType.h"

int32_t ImgBrickConsoleQB::ResetBricks = 30;
int32_t ImgBrickConsoleQB::ResetConsole = 60;
int32_t ImgBrickConsoleQB::ResetInteract = 45;

void ImgBrickConsoleQB::OnStartup(Entity* self) {
	self->SetNetworkVar(u"used", false);

	self->AddTimer("reset", ResetBricks);
}

void ImgBrickConsoleQB::OnUse(Entity* self, Entity* user) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	if (rebuildComponent->GetState() == eRebuildState::COMPLETED) {
		if (!self->GetNetworkVar<bool>(u"used")) {
			const auto consoles = Game::entityManager->GetEntitiesInGroup("Console");

			auto bothBuilt = false;

			for (auto* console : consoles) {
				auto* consoleRebuildComponent = console->GetComponent<RebuildComponent>();

				if (consoleRebuildComponent->GetState() != eRebuildState::COMPLETED) {
					continue;
				}

				console->CancelAllTimers();

				if (console->GetNetworkVar<bool>(u"used")) {
					bothBuilt = true;
				}
			}

			if (bothBuilt) {
				SmashCanister(self);
			} else {
				SpawnBrick(self);
			}

			self->AddTimer("Die", ResetInteract);

			auto onFX = 0;

			const auto location = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"console"));

			if (location == "Left") {
				onFX = 2776;
			} else {
				onFX = 2779;
			}

			const auto& facility = Game::entityManager->GetEntitiesInGroup("FacilityPipes");

			if (!facility.empty()) {
				GameMessages::SendStopFXEffect(facility[0], true, location + "PipeEnergy");
				GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), onFX, u"create", location + "PipeOn");
			}
		}

		auto* player = user;

		auto* missionComponent = player->GetComponent<MissionComponent>();
		auto* inventoryComponent = player->GetComponent<InventoryComponent>();

		if (missionComponent != nullptr && inventoryComponent != nullptr) {
			if (missionComponent->GetMissionState(1302) == eMissionState::ACTIVE) {
				inventoryComponent->RemoveItem(13074, 1);

				missionComponent->ForceProgressTaskType(1302, 1, 1);
			}

			if (missionComponent->GetMissionState(1926) == eMissionState::ACTIVE) {
				inventoryComponent->RemoveItem(14472, 1);

				missionComponent->ForceProgressTaskType(1926, 1, 1);
			}
		}

		self->SetNetworkVar(u"used", true);

		GameMessages::SendTerminateInteraction(player->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());
	}
}

void ImgBrickConsoleQB::SpawnBrick(Entity* self) {
	const auto netDevil = Game::zoneManager->GetSpawnersByName("MaelstromBug");
	if (!netDevil.empty()) {
		netDevil[0]->Reset();
		netDevil[0]->Deactivate();
	}

	const auto brick = Game::zoneManager->GetSpawnersByName("Imagination");
	if (!brick.empty()) {
		brick[0]->Activate();
	}
}

void ImgBrickConsoleQB::SmashCanister(Entity* self) {
	const auto brick = Game::entityManager->GetEntitiesInGroup("Imagination");
	if (!brick.empty()) {
		GameMessages::SendPlayFXEffect(brick[0]->GetObjectID(), 122, u"create", "bluebrick");
		GameMessages::SendPlayFXEffect(brick[0]->GetObjectID(), 1034, u"cast", "imaginationexplosion");
	}

	const auto canisters = Game::entityManager->GetEntitiesInGroup("Canister");
	for (auto* canister : canisters) {
		canister->Smash(canister->GetObjectID(), eKillType::VIOLENT);
	}

	const auto canister = Game::zoneManager->GetSpawnersByName("BrickCanister");
	if (!canister.empty()) {
		canister[0]->Reset();
		canister[0]->Deactivate();
	}
}

void ImgBrickConsoleQB::OnRebuildComplete(Entity* self, Entity* target) {
	auto energyFX = 0;

	const auto location = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"console"));

	if (location == "Left") {
		energyFX = 2775;
	} else {
		energyFX = 2778;
	}

	const auto& facility = Game::entityManager->GetEntitiesInGroup("FacilityPipes");

	if (!facility.empty()) {
		GameMessages::SendStopFXEffect(facility[0], true, location + "PipeOff");
		GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), energyFX, u"create", location + "PipeEnergy");
	}

	const auto consoles = Game::entityManager->GetEntitiesInGroup("Console");

	for (auto* console : consoles) {
		auto* consoleRebuildComponent = console->GetComponent<RebuildComponent>();

		if (consoleRebuildComponent->GetState() != eRebuildState::COMPLETED) {
			continue;
		}

		console->CancelAllTimers();
	}

	self->AddTimer("Die", ResetConsole);
}

void ImgBrickConsoleQB::OnDie(Entity* self, Entity* killer) {
	if (self->GetVar<bool>(u"Died")) {
		return;
	}

	self->CancelAllTimers();

	self->SetVar(u"Died", true);

	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	if (rebuildComponent->GetState() == eRebuildState::COMPLETED) {
		auto offFX = 0;

		const auto location = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"console"));

		if (location == "Left") {
			offFX = 2774;
		} else {
			offFX = 2777;
		}

		const auto& facility = Game::entityManager->GetEntitiesInGroup("FacilityPipes");

		if (!facility.empty()) {
			GameMessages::SendStopFXEffect(facility[0], true, location + "PipeEnergy");
			GameMessages::SendStopFXEffect(facility[0], true, location + "PipeOn");
			GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), offFX, u"create", location + "PipeOff");
			GameMessages::SendPlayFXEffect(facility[0]->GetObjectID(), 2750, u"create", location + "imagination_canister");
		}
	}

	const auto myGroup = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawner_name"));

	const auto pipeGroup = myGroup.substr(0, 10);

	const auto firstPipe = pipeGroup + "1";

	const auto samePipeSpawner = Game::zoneManager->GetSpawnersByName(myGroup);
	if (!samePipeSpawner.empty()) {
		samePipeSpawner[0]->Reset();
		samePipeSpawner[0]->Deactivate();
	}

	const auto firstPipeSpawner = Game::zoneManager->GetSpawnersByName(firstPipe);
	if (!firstPipeSpawner.empty()) {
		firstPipeSpawner[0]->Activate();
	}

	const auto netdevil = Game::zoneManager->GetSpawnersByName("Imagination");
	if (!netdevil.empty()) {
		netdevil[0]->Reset();
		netdevil[0]->Deactivate();
	}

	const auto brick = Game::zoneManager->GetSpawnersByName("MaelstromBug");
	if (!brick.empty()) {
		brick[0]->Activate();
	}

	const auto canister = Game::zoneManager->GetSpawnersByName("BrickCanister");
	if (!canister.empty()) {
		canister[0]->Activate();
	}

	self->SetNetworkVar(u"used", false);
}

void ImgBrickConsoleQB::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "reset") {
		auto* rebuildComponent = self->GetComponent<RebuildComponent>();

		if (rebuildComponent->GetState() == eRebuildState::OPEN) {
			self->Smash(self->GetObjectID(), eKillType::SILENT);
		}
	} else if (timerName == "Die") {
		const auto consoles = Game::entityManager->GetEntitiesInGroup("Console");

		for (auto* console : consoles) {
			console->Smash(console->GetObjectID(), eKillType::VIOLENT);
		}
	}
}
