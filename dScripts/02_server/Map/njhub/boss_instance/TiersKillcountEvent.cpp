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

	self->SetNetworkVar(u"enemiesalive", 1);	

//Spawn enemies	
	SpawnLegs(self, "DartBeetle");
	SpawnLegs(self, "DartTroopers1");	
	SpawnLegs(self, "DartTroopers2");	
	SpawnLegs(self, "DartTroopers3");	
	SpawnLegs(self, "DartTroopers4");	
	SpawnLegs(self, "DartTroopers5");	
	SpawnLegs(self, "DartTroopers6");
//End

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(165, "fetch_players");	
	
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

void TiersKillcountEvent::SpawnLegs(Entity* self, const std::string& loc) {
	
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

//reference LOTs here:
//	TroopersLOT = 16835
//	BeetleLOT = 13997

// Labeled Troopers but LOT crafters	

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.lot = 16835;
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;

//Enemy positions
	if (loc == "DartBeetle") {
		const auto dir = rot.GetRightVector();
		info.lot = 13997;			
		pos.x = -734.12939;
		pos.y = 293.78616;
		pos.z = -966.67633;
		info.pos = pos;
	} else if (loc == "DartTroopers1") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -690.64642;
		pos.y = 293.78619;
		pos.z = -992.8302;
		info.pos = pos;
	} else if (loc == "DartTroopers2") {
		const auto dir = rot.GetRightVector();		
		pos.x = -689.7887;
		pos.y = 293.7862;
		pos.z = -939.33551;
		info.pos = pos;
	} else if (loc == "DartTroopers3") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -734.40247;
		pos.y = 293.78613;
		pos.z = -939.37243;
		info.pos = pos;
	} else if (loc == "DartTroopers4") {
		const auto dir = rot.GetRightVector();		
		pos.x = -782.2334;
		pos.y = 293.78616;
		pos.z = -938.79578;
		info.pos = pos;
	} else if (loc == "DartTroopers5") {		
		const auto dir = rot.GetRightVector();		
		pos.x = -781.62744;
		pos.y = 293.78622;
		pos.z = -992.79541;
		info.pos = pos;
	} else if (loc == "DartTroopers6") {
		const auto dir = rot.GetRightVector();		
		pos.x = -733.32593;
		pos.y = 293.78613;
		pos.z = -992.75812;
		info.pos = pos;
	} 
//End
	
	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

	OnChildLoaded(self, entity);
}

void TiersKillcountEvent::OnChildLoaded(Entity* self, Entity* child) {
	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	legTable.push_back(child->GetObjectID());

	self->SetVar(u"legTable", legTable);

	const auto selfID = self->GetObjectID();

	child->AddDieCallback([this, selfID, child]() {
		auto* self = Game::entityManager->GetEntity(selfID);
		auto* destroyableComponent = child->GetComponent<DestroyableComponent>();

		if (destroyableComponent == nullptr || self == nullptr) {
			return;
		}		

		NotifyDie(self, child, destroyableComponent->GetKiller());
		});
}

void TiersKillcountEvent::NotifyDie(Entity* self, Entity* other, Entity* killer) {
	auto players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	const auto& iter = std::find(players.begin(), players.end(), killer->GetObjectID());

	if (iter == players.end()) {
		players.push_back(killer->GetObjectID());
	}

	self->SetVar(u"Players", players);
	

	
	OnChildRemoved(self, other);
}

void TiersKillcountEvent::OnChildRemoved(Entity* self, Entity* child) {

	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	const auto& iter = std::find(legTable.begin(), legTable.end(), child->GetObjectID());

	if (iter != legTable.end()) {
		legTable.erase(iter);
	}

	self->SetVar(u"legTable", legTable);

	if (legTable.empty()) {
		
		self->SetNetworkVar(u"enemiesalive", 0);			

		self->SetVar(u"bActive", false);				
		
//		play cinematics	for engaged players	

		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		const auto& cine = u"3Tier_EarthRailSpawn";		
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
		

		self->AddTimer("SpawnQb", 0.1f);
//		Fixed intended statue :)



	}

	auto deadLegs = self->GetVar<std::vector<std::string>>(u"DeadLegs");
	const auto& leg = child->GetVar<std::string>(u"Leg");
	const auto& legIter = std::find(deadLegs.begin(), deadLegs.end(), leg);

	if (legIter == deadLegs.end()) {
		deadLegs.push_back(leg);
	}

	self->SetVar(u"DeadLegs", deadLegs);
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
	
}

// Initialize defaults
Entity* TiersKillcountEvent::player1 = nullptr;
Entity* TiersKillcountEvent::player2 = nullptr;
Entity* TiersKillcountEvent::player3 = nullptr;
Entity* TiersKillcountEvent::player4 = nullptr;
