#include "FvMaelstromCavalry.h"
#include "EntityManager.h"

void FvMaelstromCavalry::OnStartup(Entity* self) {
	for (const auto& group : self->GetGroups()) {
		const auto& objects = EntityManager::Instance()->GetEntitiesInGroup(group);

		for (auto* obj : objects) {
			if (obj->GetLOT() != 8551) continue;

			obj->OnFireEventServerSide(self, "ISpawned");
		}
	}
}

void FvMaelstromCavalry::OnDie(Entity* self, Entity* killer) {
	if (killer == nullptr) {
		return;
	}

	if (killer->GetLOT() != 8665) {
		return;
	}

	const auto& triggers = EntityManager::Instance()->GetEntitiesInGroup("HorsemenTrigger");

	for (auto* trigger : triggers) {
		trigger->OnFireEventServerSide(self, "HorsemenDeath");
	}
}
