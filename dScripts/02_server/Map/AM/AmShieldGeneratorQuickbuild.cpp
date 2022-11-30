#include "AmShieldGeneratorQuickbuild.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "MovementAIComponent.h"
#include "BaseCombatAIComponent.h"
#include "SkillComponent.h"
#include "RebuildComponent.h"
#include "MissionComponent.h"

void AmShieldGeneratorQuickbuild::OnStartup(Entity* self) {
	self->SetProximityRadius(20, "shield");
	self->SetProximityRadius(21, "buffer");
}

void AmShieldGeneratorQuickbuild::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* destroyableComponent = entering->GetComponent<DestroyableComponent>();

	if (name == "shield") {
		if (!destroyableComponent->HasFaction(4) || entering->IsPlayer()) {
			return;
		}

		auto enemiesInProximity = self->GetVar<std::vector<LWOOBJID>>(u"Enemies");

		if (status == "ENTER") {
			EnemyEnteredShield(self, entering);

			const auto& iter = std::find(enemiesInProximity.begin(), enemiesInProximity.end(), entering->GetObjectID());

			if (iter == enemiesInProximity.end()) {
				enemiesInProximity.push_back(entering->GetObjectID());
			}
		} else if (status == "LEAVE") {
			const auto& iter = std::find(enemiesInProximity.begin(), enemiesInProximity.end(), entering->GetObjectID());

			if (iter != enemiesInProximity.end()) {
				enemiesInProximity.erase(iter);
			}
		}

		self->SetVar<std::vector<LWOOBJID>>(u"Enemies", enemiesInProximity);
	}

	if (name != "buffer" || !entering->IsPlayer()) {
		return;
	}

	auto entitiesInProximity = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	if (status == "ENTER") {
		const auto& iter = std::find(entitiesInProximity.begin(), entitiesInProximity.end(), entering->GetObjectID());

		if (iter == entitiesInProximity.end()) {
			entitiesInProximity.push_back(entering->GetObjectID());
		}
	} else if (status == "LEAVE") {
		const auto& iter = std::find(entitiesInProximity.begin(), entitiesInProximity.end(), entering->GetObjectID());

		if (iter != entitiesInProximity.end()) {
			entitiesInProximity.erase(iter);
		}
	}

	self->SetVar<std::vector<LWOOBJID>>(u"Players", entitiesInProximity);
}

void AmShieldGeneratorQuickbuild::OnDie(Entity* self, Entity* killer) {
	self->CancelAllTimers();

	auto* child = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Child"));

	if (child != nullptr) {
		child->Kill();
	}
}

void AmShieldGeneratorQuickbuild::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "BuffPlayers") {
		BuffPlayers(self);

		self->AddTimer("BuffPlayers", 3.0f);
	} else if (timerName == "PlayFX") {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 5351, u"generatorOn", "generatorOn");

		self->AddTimer("PlayFX", 1.5f);
	} else if (timerName == "RefreshEnemies") {
		auto enemiesInProximity = self->GetVar<std::vector<LWOOBJID>>(u"Enemies");

		for (const auto enemyID : enemiesInProximity) {
			auto* enemy = EntityManager::Instance()->GetEntity(enemyID);

			if (enemy != nullptr) {
				EnemyEnteredShield(self, enemy);
			}
		}

		self->AddTimer("RefreshEnemies", 1.5f);
	}
}

void AmShieldGeneratorQuickbuild::OnRebuildComplete(Entity* self, Entity* target) {
	StartShield(self);

	auto enemiesInProximity = self->GetVar<std::vector<LWOOBJID>>(u"Enemies");

	for (const auto enemyID : enemiesInProximity) {
		auto* enemy = EntityManager::Instance()->GetEntity(enemyID);

		if (enemy != nullptr) {
			enemy->Smash();
		}
	}

	auto entitiesInProximity = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	for (const auto playerID : entitiesInProximity) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player == nullptr) {
			continue;
		}

		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (missionComponent == nullptr) {
			return;
		}

		missionComponent->ForceProgressTaskType(987, 1, 1, false);
	}
}

void AmShieldGeneratorQuickbuild::StartShield(Entity* self) {
	self->AddTimer("PlayFX", 1.5f);
	self->AddTimer("BuffPlayers", 3.0f);
	self->AddTimer("RefreshEnemies", 1.5f);

	const auto myPos = self->GetPosition();
	const auto myRot = self->GetRotation();

	EntityInfo info{};
	info.lot = 13111;
	info.pos = myPos;
	info.rot = myRot;
	info.spawnerID = self->GetObjectID();

	auto* child = EntityManager::Instance()->CreateEntity(info);

	self->SetVar(u"Child", child->GetObjectID());

	BuffPlayers(self);
}

void AmShieldGeneratorQuickbuild::BuffPlayers(Entity* self) {
	auto* skillComponent = self->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	auto entitiesInProximity = self->GetVar<std::vector<LWOOBJID>>(u"Players");

	for (const auto playerID : entitiesInProximity) {
		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		skillComponent->CalculateBehavior(1200, 27024, playerID, true);
	}
}

void AmShieldGeneratorQuickbuild::EnemyEnteredShield(Entity* self, Entity* intruder) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	if (rebuildComponent == nullptr || rebuildComponent->GetState() != REBUILD_COMPLETED) {
		return;
	}

	auto* baseCombatAIComponent = intruder->GetComponent<BaseCombatAIComponent>();
	auto* movementAIComponent = intruder->GetComponent<MovementAIComponent>();

	if (baseCombatAIComponent == nullptr || movementAIComponent == nullptr) {
		return;
	}

	auto dir = intruder->GetRotation().GetForwardVector() * -1;
	dir.y += 15;
	dir.x *= 50;
	dir.z *= 50;

	// TODO: Figure out how todo knockback, I'll stun them for now

	if (NiPoint3::DistanceSquared(self->GetPosition(), movementAIComponent->GetCurrentPosition()) < 20 * 20) {
		baseCombatAIComponent->Stun(2.0f);
		movementAIComponent->SetDestination(baseCombatAIComponent->GetStartPosition());
	}

	baseCombatAIComponent->ClearThreat();
}
