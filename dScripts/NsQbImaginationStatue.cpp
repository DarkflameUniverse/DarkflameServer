#include "NsQbImaginationStatue.h"
#include "EntityManager.h"
#include "GameMessages.h"

void NsQbImaginationStatue::OnStartup(Entity* self) {

}

void NsQbImaginationStatue::OnRebuildComplete(Entity* self, Entity* target) {
	if (target == nullptr) return;

	self->SetVar(u"Player", target->GetObjectID());

	SpawnLoot(self);

	self->AddTimer("SpawnDelay", 1.5f);

	self->AddTimer("StopSpawner", 10.0f);
}

void NsQbImaginationStatue::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "SpawnDelay") {
		SpawnLoot(self);

		self->AddTimer("SpawnDelay", 1.5f);
	} else if (timerName == "StopSpawner") {
		self->CancelAllTimers();
	}
}

void NsQbImaginationStatue::SpawnLoot(Entity* self) {
	const auto playerId = self->GetVar<LWOOBJID>(u"Player");

	auto* player = EntityManager::Instance()->GetEntity(playerId);

	if (player == nullptr) return;

	GameMessages::SendDropClientLoot(player, self->GetObjectID(), 935, 0);
	GameMessages::SendDropClientLoot(player, self->GetObjectID(), 935, 0);
}
