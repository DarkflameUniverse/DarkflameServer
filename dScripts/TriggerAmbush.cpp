#include "TriggerAmbush.h"

#include "dZoneManager.h"

void TriggerAmbush::OnStartup(Entity* self) {
	self->SetProximityRadius(20, "ambush");
}

void TriggerAmbush::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name != "ambush" || status != "ENTER" || !entering->IsPlayer()) return;

	if (self->GetVar<bool>(u"triggered")) return;

	self->SetVar(u"triggered", true);

	const auto spawners = dZoneManager::Instance()->GetSpawnersByName("Ambush");

	for (auto* spawner : spawners) {
		spawner->Activate();
	}

	self->AddTimer("TriggeredTimer", 45);
}

void TriggerAmbush::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "TriggeredTimer") return;

	self->SetVar(u"triggered", false);

	const auto spawners = dZoneManager::Instance()->GetSpawnersByName("Ambush");

	for (auto* spawner : spawners) {
		spawner->Reset();

		spawner->Deactivate();
	}
}
