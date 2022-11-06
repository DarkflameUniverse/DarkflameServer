#include "FvFacilityBrick.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "EntityManager.h"

void FvFacilityBrick::OnStartup(Entity* self) {
	self->SetVar(u"ConsoleLEFTActive", false);
	self->SetVar(u"ConsoleRIGHTtActive", false);
}

void FvFacilityBrick::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	auto* brickSpawner = dZoneManager::Instance()->GetSpawnersByName("ImaginationBrick")[0];
	auto* bugSpawner = dZoneManager::Instance()->GetSpawnersByName("MaelstromBug")[0];
	auto* canisterSpawner = dZoneManager::Instance()->GetSpawnersByName("BrickCanister")[0];

	if (name == "ConsoleLeftUp") {
		GameMessages::SendStopFXEffect(self, true, "LeftPipeOff");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2775, u"create", "LeftPipeEnergy");
	} else if (name == "ConsoleLeftDown") {
		self->SetVar(u"ConsoleLEFTActive", false);

		GameMessages::SendStopFXEffect(self, true, "LeftPipeEnergy");
		GameMessages::SendStopFXEffect(self, true, "LeftPipeOn");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2774, u"create", "LeftPipeOff");
	} else if (name == "ConsoleLeftActive") {
		self->SetVar(u"ConsoleLEFTActive", true);

		GameMessages::SendStopFXEffect(self, true, "LeftPipeEnergy");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2776, u"create", "LeftPipeOn");
	}

	else if (name == "ConsoleRightUp") {
		GameMessages::SendStopFXEffect(self, true, "RightPipeOff");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2778, u"create", "RightPipeEnergy");
	} else if (name == "ConsoleRightDown") {
		self->SetVar(u"ConsoleRIGHTActive", false);

		GameMessages::SendStopFXEffect(self, true, "RightPipeEnergy");
		GameMessages::SendStopFXEffect(self, true, "RightPipeOn");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2777, u"create", "RightPipeOff");
	} else if (name == "ConsoleRightActive") {
		self->SetVar(u"ConsoleRIGHTActive", true);

		GameMessages::SendStopFXEffect(self, true, "RightPipeOff");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 2779, u"create", "RightPipeEnergy");
	}

	if (self->GetVar<bool>(u"ConsoleLEFTActive") && self->GetVar<bool>(u"ConsoleRIGHTActive")) {
		auto* object = EntityManager::Instance()->GetEntitiesInGroup("Brick")[0];

		if (object != nullptr) {
			GameMessages::SendPlayFXEffect(object->GetObjectID(), 122, u"create", "bluebrick");
			GameMessages::SendPlayFXEffect(object->GetObjectID(), 1034, u"cast", "imaginationexplosion");
		}

		object = EntityManager::Instance()->GetEntitiesInGroup("Canister")[0];

		if (object != nullptr) {
			object->Smash(self->GetObjectID(), SILENT);
		}

		canisterSpawner->Reset();
		canisterSpawner->Deactivate();
	} else if (self->GetVar<bool>(u"ConsoleLEFTActive") || self->GetVar<bool>(u"ConsoleRIGHTActive")) {
		brickSpawner->Activate();

		auto* object = EntityManager::Instance()->GetEntitiesInGroup("Brick")[0];

		if (object != nullptr) {
			GameMessages::SendStopFXEffect(object, true, "bluebrick");
		}

		bugSpawner->Reset();
		bugSpawner->Deactivate();

		canisterSpawner->Reset();
		canisterSpawner->Activate();
	} else {
		brickSpawner->Reset();
		brickSpawner->Deactivate();

		bugSpawner->Reset();
		bugSpawner->Activate();
	}
}

void FvFacilityBrick::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args != "PlayFX") {
		return;
	}

	GameMessages::SendPlayFXEffect(self->GetObjectID(), 2774, u"create", "LeftPipeOff");
	GameMessages::SendPlayFXEffect(self->GetObjectID(), 2777, u"create", "RightPipeOff");
	GameMessages::SendPlayFXEffect(self->GetObjectID(), 2750, u"create", "imagination_canister");
	GameMessages::SendPlayFXEffect(self->GetObjectID(), 2751, u"create", "canister_light_filler");
}
