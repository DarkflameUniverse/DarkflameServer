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



void BladesKillcountEvent::OnStartup(Entity* self) {



//Spawn blades enemies	
	SpawnLegs(self, "BladesElite");
	SpawnLegs(self, "BladesTroopers1");	
	SpawnLegs(self, "BladesTroopers2");	
	SpawnLegs(self, "BladesTroopers3");	
	SpawnLegs(self, "BladesTroopers4");	
	SpawnLegs(self, "BladesTroopers5");	
	SpawnLegs(self, "BladesTroopers6");	
	SpawnLegs(self, "BladesTroopers7");	
	SpawnLegs(self, "BladesTroopers8");	
	SpawnLegs(self, "BladesMarksmen1");
	SpawnLegs(self, "BladesMarksmen2");	
//End	

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(330, "fetch_players");	
	
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

void BladesKillcountEvent::SpawnLegs(Entity* self, const std::string& loc) {
	
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

//reference LOTs here:
//	TroopersLOT = 16835
//	ChopovLOT = 16851
//	MarksmenLOT = 16850

// Labeled Troopers but LOT crafters	
// Labeled Marksman but LOT savage bone wolf

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;

//Enemy positions
	if (loc == "BladesElite") {
		const auto dir = rot.GetRightVector();
		info.lot = 16851;			
		pos.x = -773.35547;
		pos.y = 241.92345;
		pos.z = -45.48052;
		info.pos = pos;
	} else if (loc == "BladesTroopers1") {		
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -846.41669;
		pos.y = 241.92349;
		pos.z = -71.09531;
		info.pos = pos;
	} else if (loc == "BladesTroopers2") {
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -843.44458;
		pos.y = 241.92346;
		pos.z = -19.31680;
		info.pos = pos;
	} else if (loc == "BladesTroopers3") {		
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -802.23376;
		pos.y = 241.92331;
		pos.z = 13.71968;
		info.pos = pos;
	} else if (loc == "BladesTroopers4") {
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -737.43726;
		pos.y = 241.92334;
		pos.z = 16.50542;
		info.pos = pos;
	} else if (loc == "BladesTroopers5") {		
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -699.76794;
		pos.y = 241.92326;
		pos.z = -21.25237;
		info.pos = pos;
	} else if (loc == "BladesTroopers6") {
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -700.76599;
		pos.y = 241.92348;
		pos.z = -77.93959;
		info.pos = pos;
	} else if (loc == "BladesTroopers7") {		
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -747.98181;
		pos.y = 241.92346;
		pos.z = -112.76208;
		info.pos = pos;
	} else if (loc == "BladesTroopers8") {
		const auto dir = rot.GetRightVector();
		info.lot = 16835;			
		pos.x = -806.22412;
		pos.y = 241.92343;
		pos.z = -110.01553;
		info.pos = pos;
	} else if (loc == "BladesMarksmen1") {
		const auto dir = rot.GetRightVector();
		info.lot = 16850;			
		pos.x = -683.0061;
		pos.y = 241.92348;
		pos.z = -47.28323;
		info.pos = pos;	
	} else if (loc == "BladesMarksmen2") {
		const auto dir = rot.GetRightVector();
		info.lot = 16850;			
		pos.x = -860.39197;
		pos.y = 242.14323;
		pos.z = -46.72139;
		info.pos = pos;		
	}
//End
	
	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

	OnChildLoaded(self, entity);
}

void BladesKillcountEvent::OnChildLoaded(Entity* self, Entity* child) {
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

void BladesKillcountEvent::NotifyDie(Entity* self, Entity* other, Entity* killer) {
	auto players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	const auto& iter = std::find(players.begin(), players.end(), killer->GetObjectID());

	if (iter == players.end()) {
		players.push_back(killer->GetObjectID());
	}

	self->SetVar(u"Players", players);
	

	
	OnChildRemoved(self, other);
}

void BladesKillcountEvent::OnChildRemoved(Entity* self, Entity* child) {

	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	const auto& iter = std::find(legTable.begin(), legTable.end(), child->GetObjectID());

	if (iter != legTable.end()) {
		legTable.erase(iter);
	}

	self->SetVar(u"legTable", legTable);

	if (legTable.empty()) {
		self->SetVar(u"bActive", false);		
	
	
//		Update music	
//		for (auto* soundBox : Game::entityManager->GetEntitiesInGroup("BladesMusicCue")) {
//			GameMessages::SendNotifyClientObject(soundBox->GetObjectID(), u"");
//		}		
	
		

//		play cinematics	for engaged players	

		auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();
		const auto& cine = self->GetVar<std::u16string>(u"cinematic");		
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
		
//		spawnDelay = 2 from original script
		self->AddTimer("SmashSelf", 0.1);
//		dlu delays first smash to spawn in worldserver by 2 secs anyways, reduce to 0.1 unless fixed


	}

	auto deadLegs = self->GetVar<std::vector<std::string>>(u"DeadLegs");
	const auto& leg = child->GetVar<std::string>(u"Leg");
	const auto& legIter = std::find(deadLegs.begin(), deadLegs.end(), leg);

	if (legIter == deadLegs.end()) {
		deadLegs.push_back(leg);
	}

	self->SetVar(u"DeadLegs", deadLegs);
}

void BladesKillcountEvent::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "SmashSelf") {
		self->Smash(self->GetObjectID());
	}	
	
}

// Initialize defaults
Entity* BladesKillcountEvent::player1 = nullptr;
Entity* BladesKillcountEvent::player2 = nullptr;
Entity* BladesKillcountEvent::player3 = nullptr;
Entity* BladesKillcountEvent::player4 = nullptr;
