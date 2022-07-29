#include "MonCoreNookDoors.h"
#include "dZoneManager.h"

void MonCoreNookDoors::OnStartup(Entity* self) {
	SpawnDoor(self);
}

void MonCoreNookDoors::SpawnDoor(Entity* self) {
	const auto doorNum = self->GetVarAsString(u"number");

	if (doorNum.empty()) {
		return;
	}

	const auto spawners = dZoneManager::Instance()->GetSpawnersByName("MonCoreNookDoor0" + doorNum);

	if (spawners.empty()) {
		return;
	}

	auto* spawner = spawners[0];

	spawner->Reset();
	spawner->Activate();
}

void MonCoreNookDoors::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "DoorSmashed") {
		self->AddTimer("RespawnDoor", 30);
	}
}

void MonCoreNookDoors::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "RespawnDoor") {
		SpawnDoor(self);
	}
}
