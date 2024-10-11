#include "BladesKillcountEvent.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "dZoneManager.h"



void BladesKillcountEvent::OnStartup(Entity* self) {	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(330, "fetch_players");	
	self->SetVar<int>(u"SmashedCount", 0);
	

//	Give spawners time to activate 	
	self->AddTimer("StartDieCallbacks", 26);	
}	

void BladesKillcountEvent::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "fetch_players") {	
		if (entering->IsPlayer()) {
			
		    if (entering != player1 && entering != player2 && entering != player3 && entering != player4) {	
				if (!player1) {
					player1 = entering;
				} else if (!player2) {
					player2 = entering;
				} else if (!player3) {
					player3 = entering;
				} else if (!player4) {
					player4 = entering;
				}
			}	
		}	
    }	
	

}

void BladesKillcountEvent::RunDieCallbacks(Entity* self) {
	auto enemyEntities = Game::entityManager->GetEntitiesInGroup("BladesEnemy");
	
	for (auto* entity : enemyEntities) {
		entity->AddDieCallback([self, this]() {
			EntitySmashed(self);
		});			
	}
}

void BladesKillcountEvent::EntitySmashed(Entity* self) {	
	auto KillCount = self->GetVar<int>(u"SmashedCount");
	
	if (KillCount == 10) {

//		play cinematics	for engaged players	

		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		const auto& cine = u"PopUpBlades_IRail_QBSpawn";		
		if (player1 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player1->GetObjectID())) {
			GameMessages::SendPlayCinematic(player1->GetObjectID(), cine, player1->GetSystemAddress());	
		}
		if (player2 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player2->GetObjectID())) {				
			GameMessages::SendPlayCinematic(player2->GetObjectID(), cine, player2->GetSystemAddress());	
		}
		if (player3 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player3->GetObjectID())) {			
			GameMessages::SendPlayCinematic(player3->GetObjectID(), cine, player3->GetSystemAddress());	
		}
		if (player4 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player4->GetObjectID())) {				
			GameMessages::SendPlayCinematic(player4->GetObjectID(), cine, player4->GetSystemAddress());	
		}	

//		change to 2 if fast server
		self->AddTimer("SpawnRail", 0.1f);		
		
	}	
	self->SetVar<int>(u"SmashedCount", KillCount + 1);	
}

void BladesKillcountEvent::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "SpawnRail") {
		const auto railSpawner = Game::zoneManager->GetSpawnersByName("BladeSpinners_IRail");
		
		for (auto* rail : railSpawner) {	
			rail->Activate();
		}	
	}	
	else if (timerName == "StartDieCallbacks") {
		RunDieCallbacks(self);
	}
}

// Initialize defaults
Entity* BladesKillcountEvent::player1 = nullptr;
Entity* BladesKillcountEvent::player2 = nullptr;
Entity* BladesKillcountEvent::player3 = nullptr;
Entity* BladesKillcountEvent::player4 = nullptr;
