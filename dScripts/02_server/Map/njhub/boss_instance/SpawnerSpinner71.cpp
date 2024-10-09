#include "SpawnerSpinner71.h"
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
#include "SoundTriggerComponent.h"

void SpawnerSpinner71::OnStartup(Entity* self) {
	IsActivated = 0;
	RenderComponent::PlayAnimation(self, u"idle");
	
//	Prox for audio	
	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();	
	self->SetProximityRadius(188, "fetch_players");		
	
    // Turn on moving platforms
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, eMovementPlatformState::Moving);
	
	SpawnLegs(self, "Spinner1");	
	SpawnLegs(self, "Spinner2");	
	SpawnLegs(self, "Spinner3");	
	SpawnLegs(self, "Spinner4");	
	SpawnLegs(self, "Spinner5");	
	SpawnLegs(self, "Spinner6");	
	SpawnLegs(self, "Spinner7");	
	SpawnLegs(self, "Spinner8");	
	
}

void SpawnerSpinner71::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "fetch_players") {	
		if (entering->IsPlayer()) {
			
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

void SpawnerSpinner71::SpawnLegs(Entity* self, const std::string& loc) {
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.lot = 32100;
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;


	if (loc == "Beetle1") {
		info.lot = 13997;		
		const auto dir = rot.GetRightVector();		
		pos.x = -1726.9757;
		pos.y = 328.5881;
		pos.z = -121.266;
		info.pos = pos;
	} else if (loc == "Beetle2") {
		info.lot = 13997;	
		const auto dir = rot.GetRightVector();		
		pos.x = -1742.26697;
		pos.y = 328.5881;
		pos.z = -103.42309;
		info.pos = pos; 	
	} else if (loc == "Spinner1") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1620.75806;
		pos.y = 363.62388;
		pos.z = -58.51917;
		info.pos = pos;	
	} else if (loc == "Spinner2") {	
		const auto dir = rot.GetRightVector();		
		pos.x = -1627.15234;
		pos.y = 353.26551;
		pos.z = 10.39728;
		info.pos = pos;
	} else if (loc == "Spinner3") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1648.29675;
		pos.y = 378.76255;
		pos.z = 3.63825;
		info.pos = pos;
	} else if (loc == "Spinner4") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1681.39697;
		pos.y = 367.84492;
		pos.z = -230.77191;
		info.pos = pos;
	} else if (loc == "Spinner5") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1782.59766;
		pos.y = 378.79169;
		pos.z = 4.36175;
		info.pos = pos;
	} else if (loc == "Spinner6") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1846.72534;
		pos.y = 365.9621;
		pos.z = -64.57022;
		info.pos = pos;
	} else if (loc == "Spinner7") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1847.27747;
		pos.y = 365.09278;
		pos.z = -191.28635;
		info.pos = pos;
	} else if (loc == "Spinner8") {
		const auto dir = rot.GetRightVector();		
		pos.x = -1849.40466;
		pos.y = 330.00284;
		pos.z = -230.1709;
		info.pos = pos;
	}

	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());
	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);
	
	OnChildLoaded(self, entity);
}

void SpawnerSpinner71::OnChildLoaded(Entity* self, Entity* child) {
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

void SpawnerSpinner71::NotifyDie(Entity* self, Entity* other, Entity* killer) {
	auto players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	const auto& iter = std::find(players.begin(), players.end(), killer->GetObjectID());

	if (iter == players.end()) {
		players.push_back(killer->GetObjectID());
	}

	self->SetVar(u"Players", players);
	

	
	OnChildRemoved(self, other);
}

void SpawnerSpinner71::OnChildRemoved(Entity* self, Entity* child) {

	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	const auto& iter = std::find(legTable.begin(), legTable.end(), child->GetObjectID());

	if (iter != legTable.end()) {
		legTable.erase(iter);
	}

	self->SetVar(u"legTable", legTable);

	if (legTable.empty()) {
		
//		start audio sequence for proper players	

		self->SetNetworkVar(u"flourishcue", 1);
		self->AddTimer("PlayerFlourish", 4.8f);	
	
		
//		Start spawner		
		if (IsActivated == 0) {
			self->AddTimer("SpawnerActivate", 3.3f);			
			IsActivated = 1;				
		}

	}

	auto deadLegs = self->GetVar<std::vector<std::string>>(u"DeadLegs");
	const auto& leg = child->GetVar<std::string>(u"Leg");
	const auto& legIter = std::find(deadLegs.begin(), deadLegs.end(), leg);

	if (legIter == deadLegs.end()) {
		deadLegs.push_back(leg);
	}

	self->SetVar(u"DeadLegs", deadLegs);
}

void SpawnerSpinner71::OnTimerDone(Entity* self, std::string timerName) {
		

	if (timerName == "PlayerFlourish") {	
		self->SetNetworkVar(u"triggerflourish", 1);			
	}	
	
	
	
	if (timerName == "MoveUp") {		
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
		RenderComponent::PlayAnimation(self, u"up");	
		
//		Ascend sfx
		GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{5c30c263-00ae-42a2-80a3-2ae33c8f13fe}");	
		self->AddTimer("AscentGUID", 0.1f);		

	} else if (timerName == "SpawnBeetles") {
		SpawnLegs(self, "Beetle1");	
		SpawnLegs(self, "Beetle2");
	} else if (timerName == "SpawnVultures") {	
		const auto spawnNw = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"spawnNw1"));	
		auto spawners = Game::zoneManager->GetSpawnersByName(spawnNw);	
		if (!spawners.empty()) {
			auto* spawner = spawners.front();
			spawner->Activate();
		}		
	} else if (timerName == "SpawnerActivate") {	

		
//		Spawner network spawns at door even though spinner is in mid of room
//		Here's an optional beetles spawn to match with the spawnin FX
//		self->AddTimer("SpawnBeetles", 3.1f);

		self->AddTimer("SpawnVultures", 3.1f);			
		self->AddTimer("MoveUp", 0.9f);	
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 10102, u"create", "create");	
		
		
	}
}


// Initialize defaults
Entity* SpawnerSpinner71::player1 = nullptr;
Entity* SpawnerSpinner71::player2 = nullptr;
Entity* SpawnerSpinner71::player3 = nullptr;
Entity* SpawnerSpinner71::player4 = nullptr;