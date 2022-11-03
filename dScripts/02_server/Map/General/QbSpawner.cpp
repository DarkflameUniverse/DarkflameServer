#include "QbSpawner.h"
#include "BaseCombatAIComponent.h"
#include "MovementAIComponent.h"

void QbSpawner::OnStartup(Entity* self) {
	auto mobNum = self->GetVar<int>(u"mobNum");
	auto spawnDist = self->GetVar<float>(u"spawnDist");
	auto mobTemplate = self->GetVar<LWOOBJID>(u"mobTemplate");
	auto spawnTime = self->GetVar<float>(u"spawnTime");

	if (!mobNum) self->SetVar<int>(u"mobNum", m_DefaultMobNum);
	if (!spawnDist) self->SetVar<float>(u"spawnDist", m_DefaultSpawnDist);
	if (!mobTemplate) self->SetVar<LWOOBJID>(u"mobTemplate", m_DefaultMobTemplate);
	if (!spawnTime) self->SetVar<float>(u"spawnTime", m_DefaultSpawnTime);

	// go ahead and setup the mob table here
	std::vector<LWOOBJID> mobTable;
	mobTable.assign(self->GetVar<int>(u"mobNum"), LWOOBJID_EMPTY);

	self->SetVar<std::vector<LWOOBJID>>(u"mobTable", mobTable);
}

void QbSpawner::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	auto gateObjID = sender->GetObjectID();
	if (!gateObjID) return;
	if (args == "spawnMobs") {
		self->SetVar(u"gateObj", gateObjID);
		auto spawnTime = self->GetVar<float>(u"spawnTime");
		self->AddTimer("SpawnMobEnemies", spawnTime);
	}
}

void QbSpawner::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "SpawnMobEnemies") {
		auto mobTable = self->GetVar<std::vector<LWOOBJID>>(u"mobTable");

		auto spawnDist = self->GetVar<float>(u"spawnDist");
		auto mobTemplate = self->GetVar<LWOOBJID>(u"mobTemplate");

		auto gateObjID = self->GetVar<LWOOBJID>(u"gateObj");
		if (!gateObjID) return;

		auto* gate = EntityManager::Instance()->GetEntity(gateObjID);
		if (!gate) return;

		auto oPos = gate->GetPosition();
		auto oDir = gate->GetRotation().GetForwardVector();
		NiPoint3 newPos(
			oPos.x + (oDir.x * spawnDist),
			oPos.y,
			oPos.z + (oDir.z * spawnDist)
		);
		auto newRot = NiQuaternion::LookAt(newPos, oPos);

		for (int i = 0; i < mobTable.size(); i++) {
			int posOffset = -10;
			if (mobTable[i] == LWOOBJID_EMPTY) {
				posOffset = posOffset + 5 * i;
				auto newOffset = newPos;
				newOffset.z = newOffset.z + posOffset;

				EntityInfo info{};
				info.lot = mobTemplate;
				info.pos = newOffset;
				info.rot = newRot;
				info.spawnerID = self->GetObjectID();
				info.spawnerNodeID = 0;
				info.settings = {
					new LDFData<bool>(u"no_timed_spawn", true),
					new LDFData<float>(u"aggroRadius", 70),
					new LDFData<float>(u"softtetherRadius", 80),
					new LDFData<float>(u"tetherRadius", 90),
					new LDFData<float>(u"wanderRadius", 5),
					new LDFData<int>(u"mobTableLoc", i)
				};

				auto* child = EntityManager::Instance()->CreateEntity(info, nullptr, self);
				EntityManager::Instance()->ConstructEntity(child);

				OnChildLoaded(self, child);
			} else {
				auto* mob = EntityManager::Instance()->GetEntity(mobTable[i]);
				AggroTargetObject(self, mob);
			}
		}

	}
}

void QbSpawner::OnChildLoaded(Entity* self, Entity* child) {
	auto mobTable = self->GetVar<std::vector<LWOOBJID>>(u"mobTable");
	auto tableLoc = child->GetVar<int>(u"mobTableLoc");

	mobTable[tableLoc] = child->GetObjectID();
	self->SetVar<std::vector<LWOOBJID>>(u"mobTable", mobTable);

	AggroTargetObject(self, child);

	const auto selfID = self->GetObjectID();

	child->AddDieCallback([this, selfID, child]() {
		auto* self = EntityManager::Instance()->GetEntity(selfID);
		OnChildRemoved(self, child);
		}
	);
}

void QbSpawner::OnChildRemoved(Entity* self, Entity* child) {
	auto mobTable = self->GetVar<std::vector<LWOOBJID>>(u"mobTable");
	auto tableLoc = child->GetVar<int>(u"mobTableLoc");

	mobTable[tableLoc] = LWOOBJID_EMPTY;
	self->SetVar<std::vector<LWOOBJID>>(u"mobTable", mobTable);
}

void QbSpawner::AggroTargetObject(Entity* self, Entity* enemy) {
	auto* baseCombatAIComponent = enemy->GetComponent<BaseCombatAIComponent>();
	if (!baseCombatAIComponent) return;

	auto gateObjID = self->GetVar<LWOOBJID>(u"gateObj");
	if (gateObjID) {
		auto* gate = EntityManager::Instance()->GetEntity(gateObjID);
		if (gate) {
			auto* movementAIComponent = enemy->GetComponent<MovementAIComponent>();
			if (movementAIComponent) movementAIComponent->SetDestination(gate->GetPosition());
			baseCombatAIComponent->Taunt(gateObjID, 1000);
		}
	}

	auto playerObjID = self->GetVar<LWOOBJID>(u"player");
	if (playerObjID) {
		baseCombatAIComponent->Taunt(playerObjID, 100);
	}

}

