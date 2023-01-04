#include "Lieutenant.h"
#include "SkillComponent.h"
#include "dZoneManager.h"

void Lieutenant::OnStartup(Entity* self) {
	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(1127, 24812, self->GetObjectID(), true);
}

void Lieutenant::OnDie(Entity* self, Entity* killer) {
	const auto myLOT = self->GetLOT();

	std::string spawnerName;

	switch (myLOT) {
	case 16047:
		spawnerName = "EarthShrine_ERail";
		break;
	case 16050:
		spawnerName = "IceShrine_QBBouncer";
		break;
	case 16049:
		spawnerName = "LightningShrine_LRail";
		break;
	default:
		return;
	}

	const auto spawners = dZoneManager::Instance()->GetSpawnersByName(spawnerName);

	if (spawners.empty()) {
		return;
	}

	for (auto* spawner : spawners) {
		spawner->Reset();
		spawner->Activate();
	}
}
