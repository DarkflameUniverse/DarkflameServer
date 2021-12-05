#include "TriggerAmbush.h"

#include "dZoneManager.h"
#include "EntityManager.h"

void TriggerAmbush::OnStartup(Entity* self)
{
	self->SetProximityRadius(20, "ambush");
}

void TriggerAmbush::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status)
{
	if (name != "ambush" || status != "ENTER" || !entering->IsPlayer()) return;
	
	Game::logger->Log("TriggerAmbush", "Got ambush collision!\n");

	if (self->GetVar<bool>(u"triggered")) return;

	self->SetVar(u"triggered", true);

	Game::logger->Log("TriggerAmbush", "Triggering ambush!\n");

	const auto spawners = dZoneManager::Instance()->GetSpawnersByName("Ambush");

	for (auto* spawner : spawners)
	{
		Game::logger->Log("TriggerAmbush", "Triggering ambush on spawner!\n");

		spawner->Activate();
	}

	self->AddTimer("TriggeredTimer", 45);
}

void TriggerAmbush::OnTimerDone(Entity* self, std::string timerName)
{
	if (timerName != "TriggeredTimer") return;

	self->SetVar(u"triggered", false);

	const auto spawners = dZoneManager::Instance()->GetSpawnersByName("Ambush");

	for (auto* spawner : spawners)
	{
		spawner->Reset();
		
		spawner->Deactivate();
	}
}
