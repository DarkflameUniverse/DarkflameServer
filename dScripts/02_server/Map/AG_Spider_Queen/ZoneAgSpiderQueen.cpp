#include "ZoneAgSpiderQueen.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "ZoneAgProperty.h"
#include "DestroyableComponent.h"

void ZoneAgSpiderQueen::SetGameVariables(Entity* self) {
	ZoneAgProperty::SetGameVariables(self);

	// Disable property flags
	self->SetVar<uint32_t>(defeatedProperyFlag, 0);
	self->SetVar<uint32_t>(placedModelFlag, 0);
	self->SetVar<uint32_t>(guardFirstMissionFlag, 0);
	self->SetVar<uint32_t>(guardMissionFlag, 0);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 0);
}

void ZoneAgSpiderQueen::OnStartup(Entity* self) {
	LoadInstance(self);

	SpawnSpots(self);
	StartMaelstrom(self, nullptr);
}

void ZoneAgSpiderQueen::BasePlayerLoaded(Entity* self, Entity* player) {
	ActivityManager::UpdatePlayer(self, player->GetObjectID());
	ActivityManager::TakeActivityCost(self, player->GetObjectID());

	// Make sure the player has full stats when they join
	auto* playerDestroyableComponent = player->GetComponent<DestroyableComponent>();
	if (playerDestroyableComponent != nullptr) {
		playerDestroyableComponent->SetImagination(playerDestroyableComponent->GetMaxImagination());
		playerDestroyableComponent->SetArmor(playerDestroyableComponent->GetMaxArmor());
		playerDestroyableComponent->SetHealth(playerDestroyableComponent->GetMaxHealth());
	}

	self->SetNetworkVar(u"unclaimed", true);
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"maelstromSkyOn", 0, 0, LWOOBJID_EMPTY,
		"", player->GetSystemAddress());
}

void
ZoneAgSpiderQueen::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "ClearProperty") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlayCinematic", 0, 0,
			LWOOBJID_EMPTY, destroyedCinematic, UNASSIGNED_SYSTEM_ADDRESS);
		self->AddTimer("tornadoOff", 0.5f);
	} else {
		ZoneAgProperty::BaseOnFireEventServerSide(self, sender, args);
	}
}

void ZoneAgSpiderQueen::OnPlayerExit(Entity* self, Entity* player) {
	UpdatePlayer(self, player->GetObjectID(), true);
}

void ZoneAgSpiderQueen::OnTimerDone(Entity* self, std::string timerName) {

	// Disable some stuff from the regular property
	if (timerName == "BoundsVisOn" || timerName == "GuardFlyAway" || timerName == "ShowVendor")
		return;

	if (timerName == "killSpider") {
		auto spawnTargets = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(LandTargetGroup));
		for (auto* spawnTarget : spawnTargets) {
			EntityInfo info{};

			info.spawnerID = spawnTarget->GetObjectID();
			info.pos = spawnTarget->GetPosition();
			info.rot = spawnTarget->GetRotation();
			info.lot = chestObject;
			info.settings = {
					new LDFData<LWOOBJID>(u"parent_tag", self->GetObjectID())
			};

			auto* chest = EntityManager::Instance()->CreateEntity(info);
			EntityManager::Instance()->ConstructEntity(chest);
		}
	}

	ZoneAgProperty::BaseTimerDone(self, timerName);
}
