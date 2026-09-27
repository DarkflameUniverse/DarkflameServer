#include "FvFacilityBrick.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "dZoneManager.h"
#include "EntityManager.h"

void FvFacilityBrick::OnStartup(Entity* self) {
	self->SetVar(u"ConsoleLEFTActive", false);
	self->SetVar(u"ConsoleRIGHTtActive", false);
}

void FvFacilityBrick::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	const auto brickObjs = Game::zoneManager->GetSpawnersByName("ImaginationBrick");
	auto* const brickSpawner = brickObjs.empty() ? nullptr : brickObjs[0];
	const auto bugObjs = Game::zoneManager->GetSpawnersByName("MaelstromBug");
	auto* const bugSpawner = bugObjs.empty() ? nullptr : bugObjs[0];
	const auto canisterObjs = Game::zoneManager->GetSpawnersByName("BrickCanister");
	auto* const canisterSpawner = canisterObjs.empty() ? nullptr : canisterObjs[0];

	if (name == "ConsoleLeftUp") {
		GameMessages::StopFXEffect(self->GetObjectID(), true, "LeftPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::PlayFXEffect(self->GetObjectID(), 2775, u"create", "LeftPipeEnergy").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (name == "ConsoleLeftDown") {
		self->SetVar(u"ConsoleLEFTActive", false);

		GameMessages::StopFXEffect(self->GetObjectID(), true, "LeftPipeEnergy").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::StopFXEffect(self->GetObjectID(), true, "LeftPipeOn").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::PlayFXEffect(self->GetObjectID(), 2774, u"create", "LeftPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (name == "ConsoleLeftActive") {
		self->SetVar(u"ConsoleLEFTActive", true);

		GameMessages::StopFXEffect(self->GetObjectID(), true, "LeftPipeEnergy").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::PlayFXEffect(self->GetObjectID(), 2776, u"create", "LeftPipeOn").Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	else if (name == "ConsoleRightUp") {
		GameMessages::StopFXEffect(self->GetObjectID(), true, "RightPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::PlayFXEffect(self->GetObjectID(), 2778, u"create", "RightPipeEnergy").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (name == "ConsoleRightDown") {
		self->SetVar(u"ConsoleRIGHTActive", false);

		GameMessages::StopFXEffect(self->GetObjectID(), true, "RightPipeEnergy").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::StopFXEffect(self->GetObjectID(), true, "RightPipeOn").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::PlayFXEffect(self->GetObjectID(), 2777, u"create", "RightPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
	} else if (name == "ConsoleRightActive") {
		self->SetVar(u"ConsoleRIGHTActive", true);

		GameMessages::StopFXEffect(self->GetObjectID(), true, "RightPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
		GameMessages::PlayFXEffect(self->GetObjectID(), 2779, u"create", "RightPipeEnergy").Send(UNASSIGNED_SYSTEM_ADDRESS);
	}

	if (self->GetVar<bool>(u"ConsoleLEFTActive") && self->GetVar<bool>(u"ConsoleRIGHTActive")) {
		auto* object = Game::entityManager->GetEntitiesInGroup("Brick")[0];

		if (object != nullptr) {
			GameMessages::PlayFXEffect(object->GetObjectID(), 122, u"create", "bluebrick").Send(UNASSIGNED_SYSTEM_ADDRESS);
			GameMessages::PlayFXEffect(object->GetObjectID(), 1034, u"cast", "imaginationexplosion").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		object = Game::entityManager->GetEntitiesInGroup("Canister")[0];

		if (object != nullptr) {
			object->Smash(self->GetObjectID(), eKillType::SILENT);
		}

		canisterSpawner->Reset();
		canisterSpawner->Deactivate();
	} else if (self->GetVar<bool>(u"ConsoleLEFTActive") || self->GetVar<bool>(u"ConsoleRIGHTActive")) {
		if (brickSpawner) brickSpawner->Activate();

		auto* object = Game::entityManager->GetEntitiesInGroup("Brick")[0];

		if (object != nullptr) {
			GameMessages::StopFXEffect(object->GetObjectID(), true, "bluebrick").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}

		if (bugSpawner) {
			bugSpawner->Reset();
			bugSpawner->Deactivate();
		}

		if (canisterSpawner) {
			canisterSpawner->Reset();
			canisterSpawner->Activate();
		}
	} else {
		if (brickSpawner) {
			brickSpawner->Reset();
			brickSpawner->Deactivate();
		}

		if (bugSpawner) {
			bugSpawner->Reset();
			bugSpawner->Activate();
		}
	}
}

void FvFacilityBrick::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args != "PlayFX") {
		return;
	}

	GameMessages::PlayFXEffect(self->GetObjectID(), 2774, u"create", "LeftPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::PlayFXEffect(self->GetObjectID(), 2777, u"create", "RightPipeOff").Send(UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::PlayFXEffect(self->GetObjectID(), 2750, u"create", "imagination_canister").Send(UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::PlayFXEffect(self->GetObjectID(), 2751, u"create", "canister_light_filler").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
