#include "TiersKillcountEvent.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"
#include "Entity.h"
#include "SoundTriggerComponent.h"
#include "dZoneManager.h"


// Send cinematic to all players within a proximity radius of the room's size 


void TiersKillcountEvent::OnStartup(Entity* self) {	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(165, "fetch_players");	
	
	self->SetNetworkVar(u"enemiesalive", 1);
	self->SetVar<int>(u"SmashedCount", 0);
	
//	Give spawners time to activate 	
	self->AddTimer("StartDieCallbacks", 28);		
}


void TiersKillcountEvent::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
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

void TiersKillcountEvent::RunDieCallbacks(Entity* self) {	
	auto enemyEntities = Game::entityManager->GetEntitiesInGroup("3TiersEnemy");
	
	for (auto* entity : enemyEntities) {
		entity->AddDieCallback([self, this]() {
			EntitySmashed(self);
		});			
	}	
	
}

void TiersKillcountEvent::EntitySmashed(Entity* self) {
	auto KillCount = self->GetVar<int>(u"SmashedCount");

	if (KillCount == 6) {				
//		play cinematics	for engaged players	
		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		const auto& cine = u"3Tier_EarthRailSpawn";		
		if (player1 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player1->GetObjectID())) {
			GameMessages::SendPlayCinematic(player1->GetObjectID(), cine, player1->GetSystemAddress());	
		} if (player2 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player2->GetObjectID())) {				
			GameMessages::SendPlayCinematic(player2->GetObjectID(), cine, player2->GetSystemAddress());	
		} if (player3 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player3->GetObjectID())) {			
			GameMessages::SendPlayCinematic(player3->GetObjectID(), cine, player3->GetSystemAddress());	
		} if (player4 != nullptr && proximityMonitorComponent->IsInProximity("fetch_players", player4->GetObjectID())) {				
			GameMessages::SendPlayCinematic(player4->GetObjectID(), cine, player4->GetSystemAddress());	
		}
		
		self->SetNetworkVar(u"enemiesalive", 0);			
		self->SetVar(u"bActive", false);

		self->AddTimer("SpawnQb", 0.1f);
//		Restored intended statue :)		
	}
	self->SetVar<int>(u"SmashedCount", KillCount + 1);	
}	

void TiersKillcountEvent::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "SpawnQb") {	
		const auto spawnNw = GeneralUtils::UTF16ToWTF8(u"DartSpinEarthRail");	
		auto spawners = Game::zoneManager->GetSpawnersByName(spawnNw);	
		if (!spawners.empty()) {
			auto* spawner = spawners.front();
			spawner->Activate();
		}		
	}
	else if (timerName == "StartDieCallbacks") {
		RunDieCallbacks(self);
	}	
	
}

// Initialize defaults
Entity* TiersKillcountEvent::player1 = nullptr;
Entity* TiersKillcountEvent::player2 = nullptr;
Entity* TiersKillcountEvent::player3 = nullptr;
Entity* TiersKillcountEvent::player4 = nullptr;
