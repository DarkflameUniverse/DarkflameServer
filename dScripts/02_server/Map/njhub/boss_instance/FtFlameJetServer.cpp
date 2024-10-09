#include "FtFlameJetServer.h"
#include "DestroyableComponent.h"
#include "SkillComponent.h"
#include "GameMessages.h"
#include "EntityInfo.h"
#include "EntityManager.h"


void FtFlameJetServer::OnStartup(Entity* self) {
	GameMessages::SendPlayNDAudioEmitter(self, self->GetSystemAddress(), "{49db2221-b4c7-414d-886e-6640e092f3b4}");
	self->SetNetworkVar<bool>(u"FlameOn", true);
	SpawnLegs(self);	
}

void FtFlameJetServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer()) {
		return;
	}

	if (!self->GetNetworkVar<bool>(u"FlameOn")) {
		return;
	}

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(726, 11723, target->GetObjectID(), true);

	auto dir = target->GetRotation().GetForwardVector();

	dir.y = 25;
	dir.x = -dir.x * 15;
	dir.z = -dir.z * 15;

	GameMessages::SendKnockback(target->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
}

void FtFlameJetServer::SpawnLegs(Entity* self) {
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

	EntityInfo info{};
	info.lot = 32000;
	info.spawnerID = self->GetObjectID();
	info.rot = newRot;
	
	auto legLocX = self->GetVar<float>(u"legLocX");
	auto legLocY = self->GetVar<float>(u"legLocY");
	auto legLocZ = self->GetVar<float>(u"legLocZ");
	

// if spinner up to down: pos.y = Spinner starting pos (aka Waypoint 1) - 0.2
//if spinner down to up: pos.y = Spinner starting pos (aka Waypoint 1) - 0.1

	const auto dir = rot.GetRightVector();
	pos.x = legLocX;
	pos.y = legLocY;
	pos.z = legLocZ;
	info.pos = pos;

	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());

	auto* entity = Game::entityManager->CreateEntity(info);

	Game::entityManager->ConstructEntity(entity);

	OnChildLoaded(self, entity);
}

void FtFlameJetServer::OnChildLoaded(Entity* self, Entity* child) {
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

void FtFlameJetServer::NotifyDie(Entity* self, Entity* other, Entity* killer) {
	auto players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	const auto& iter = std::find(players.begin(), players.end(), killer->GetObjectID());

	if (iter == players.end()) {
		players.push_back(killer->GetObjectID());
	}

	self->SetVar(u"Players", players);

	OnChildRemoved(self, other);
}

void FtFlameJetServer::OnChildRemoved(Entity* self, Entity* child) {
	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	const auto& iter = std::find(legTable.begin(), legTable.end(), child->GetObjectID());

	if (iter != legTable.end()) {
		legTable.erase(iter);
	}

	self->SetVar(u"legTable", legTable);

	if (legTable.empty()) {
		self->AddTimer("FlamesOff", 2);	
	}
}

void FtFlameJetServer::OnTimerDone(Entity* self, std::string timerName) {	
	if (timerName == "FlamesOff") {	
		self->SetNetworkVar<bool>(u"FlameOn", false);	
		self->AddTimer("SmashSelf", 2);	
	}
	if (timerName == "SmashSelf") {	
		self->Smash(self->GetObjectID(), eKillType::SILENT);
	}
}



