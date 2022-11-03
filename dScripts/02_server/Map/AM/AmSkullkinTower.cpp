#include "AmSkullkinTower.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "GameMessages.h"
#include "MissionComponent.h"

void AmSkullkinTower::OnStartup(Entity* self) {
	self->SetProximityRadius(20, "Tower");

	// onPhysicsComponentReady

	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();

	if (movingPlatformComponent != nullptr) {
		movingPlatformComponent->StopPathing();
	}

	SpawnLegs(self, "Left");
	SpawnLegs(self, "Right");
	SpawnLegs(self, "Rear");
}

void AmSkullkinTower::SpawnLegs(Entity* self, const std::string& loc) {
	auto pos = self->GetPosition();
	auto rot = self->GetRotation();
	pos.y += self->GetVarAs<float>(u"vert_offset");

	auto newRot = rot;
	auto offset = self->GetVarAs<float>(u"hort_offset");

	auto legLOT = self->GetVar<LOT>(u"legLOT");

	if (legLOT == 0) {
		return;
	}

	std::vector<LDFBaseData*> config = { new LDFData<std::string>(u"Leg", loc) };

	EntityInfo info{};
	info.lot = legLOT;
	info.spawnerID = self->GetObjectID();
	info.settings = config;
	info.rot = newRot;

	if (loc == "Right") {
		const auto dir = rot.GetForwardVector();
		pos.x += dir.x * offset;
		pos.z += dir.z * offset;
		info.pos = pos;
	} else if (loc == "Rear") {
		const auto dir = rot.GetRightVector();
		pos.x += dir.x * offset;
		pos.z += dir.z * offset;
		info.pos = pos;
	} else if (loc == "Left") {
		const auto dir = rot.GetForwardVector() * -1;
		pos.x += dir.x * offset;
		pos.z += dir.z * offset;
		info.pos = pos;
	}

	info.rot = NiQuaternion::LookAt(info.pos, self->GetPosition());

	auto* entity = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(entity);

	OnChildLoaded(self, entity);
}

void AmSkullkinTower::OnChildLoaded(Entity* self, Entity* child) {
	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	legTable.push_back(child->GetObjectID());

	self->SetVar(u"legTable", legTable);

	const auto selfID = self->GetObjectID();

	child->AddDieCallback([this, selfID, child]() {
		auto* self = EntityManager::Instance()->GetEntity(selfID);
		auto* destroyableComponent = child->GetComponent<DestroyableComponent>();

		if (destroyableComponent == nullptr || self == nullptr) {
			return;
		}

		NotifyDie(self, child, destroyableComponent->GetKiller());
		});
}

void AmSkullkinTower::NotifyDie(Entity* self, Entity* other, Entity* killer) {
	auto players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	const auto& iter = std::find(players.begin(), players.end(), killer->GetObjectID());

	if (iter == players.end()) {
		players.push_back(killer->GetObjectID());
	}

	self->SetVar(u"Players", players);

	OnChildRemoved(self, other);
}

void AmSkullkinTower::OnChildRemoved(Entity* self, Entity* child) {
	auto legTable = self->GetVar<std::vector<LWOOBJID>>(u"legTable");

	const auto& iter = std::find(legTable.begin(), legTable.end(), child->GetObjectID());

	if (iter != legTable.end()) {
		legTable.erase(iter);
	}

	self->SetVar(u"legTable", legTable);

	if (legTable.size() == 2) {
		GameMessages::SendPlayAnimation(self, u"wobble-1");
	} else if (legTable.size() == 1) {
		GameMessages::SendPlayAnimation(self, u"wobble-2");
	} else if (legTable.empty()) {
		const auto animTime = 2.5f;

		GameMessages::SendPlayAnimation(self, u"fall");

		self->AddTimer("spawnGuys", animTime - 0.2f);

		self->CancelTimer("RespawnLeg");
		self->CancelTimer("RespawnLeg");
		self->CancelTimer("RespawnLeg");

		std::vector<int32_t> missionIDs;

		auto missionsString = self->GetVar<std::u16string>(u"missions");

		if (!missionsString.empty()) {
			// Split the missions string by '_'
			const auto missions = GeneralUtils::SplitString(
				GeneralUtils::UTF16ToWTF8(missionsString),
				'_'
			);

			for (const auto& mission : missions) {
				int32_t missionID = 0;

				if (!GeneralUtils::TryParse(mission, missionID)) {
					continue;
				}

				missionIDs.push_back(missionID);
			}
		}

		const auto& players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

		for (const auto& playerID : players) {
			auto* player = EntityManager::Instance()->GetEntity(playerID);

			if (player == nullptr) {
				continue;
			}

			auto* missionComponent = player->GetComponent<MissionComponent>();

			if (missionComponent == nullptr) {
				continue;
			}

			for (const auto missionID : missionIDs) {
				missionComponent->ForceProgressValue(missionID, 1, self->GetLOT());
			}

			//missionComponent->ForceProgressValue(1305, 1, self->GetLOT());
		}
	}

	auto deadLegs = self->GetVar<std::vector<std::string>>(u"DeadLegs");

	const auto& leg = child->GetVar<std::string>(u"Leg");

	const auto& legIter = std::find(deadLegs.begin(), deadLegs.end(), leg);

	if (legIter == deadLegs.end()) {
		deadLegs.push_back(leg);
	}

	self->SetVar(u"DeadLegs", deadLegs);

	self->AddTimer("RespawnLeg", 20);
}

void AmSkullkinTower::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (status != "LEAVE") {
		return;
	}

	auto players = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	const auto& iter = std::find(players.begin(), players.end(), entering->GetObjectID());

	if (iter != players.end()) {
		players.erase(iter);
	}

	self->SetVar(u"Players", players);
}

void AmSkullkinTower::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "RespawnLeg") {
		auto deadLegs = self->GetVar<std::vector<std::string>>(u"DeadLegs");

		if (deadLegs.empty()) {
			return;
		}

		SpawnLegs(self, deadLegs[0]);

		deadLegs.erase(deadLegs.begin());

		self->SetVar<std::vector<std::string>>(u"DeadLegs", deadLegs);
	} else if (timerName == "spawnGuys") {
		EntityInfo info{};
		info.lot = self->GetVar<LOT>(u"enemyToSpawn");
		auto pos = self->GetPosition();
		pos.y += 7;
		info.pos = pos;
		info.rot = self->GetRotation();
		info.spawnerID = self->GetObjectID();

		for (size_t i = 0; i < 2; i++) {
			info.pos.x += i * 2; // Just to set the apart a bit

			auto* entity = EntityManager::Instance()->CreateEntity(info);

			EntityManager::Instance()->ConstructEntity(entity);
		}

		self->AddTimer("killTower", 0.7f);
	} else if (timerName == "killTower") {
		self->Smash(self->GetObjectID());
	}
}
