#include "NsQbImaginationStatue.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "Loot.h"

void NsQbImaginationStatue::OnStartup(Entity* self) {

}

void NsQbImaginationStatue::OnQuickBuildComplete(Entity* self, Entity* target) {
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

	auto* player = Game::entityManager->GetEntity(playerId);

	if (player == nullptr) return;

	GameMessages::DropClientLoot lootMsg{};
	lootMsg.target = player->GetObjectID();
	lootMsg.ownerID = player->GetObjectID();
	lootMsg.sourceID = self->GetObjectID();
	lootMsg.item = 935;
	lootMsg.count = 1;
	Loot::DropItem(*player, lootMsg);
	Loot::DropItem(*player, lootMsg);
}
