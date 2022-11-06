#include "FvMaelstromDragon.h"
#include "EntityManager.h"
#include "SkillComponent.h"
#include "BaseCombatAIComponent.h"
#include "DestroyableComponent.h"

void FvMaelstromDragon::OnStartup(Entity* self) {
	self->SetVar<int32_t>(u"weakspot", 0);

	auto* baseCombatAIComponent = self->GetComponent<BaseCombatAIComponent>();

	if (baseCombatAIComponent != nullptr) {
		baseCombatAIComponent->SetStunImmune(true);
	}
}

void FvMaelstromDragon::OnDie(Entity* self, Entity* killer) {
	if (self->GetVar<bool>(u"bDied")) {
		return;
	}

	self->SetVar<bool>(u"bDied", true);

	auto position = self->GetPosition();
	auto rotation = self->GetRotation();

	auto chestObject = 11229;

	EntityInfo info{};
	info.lot = chestObject;
	info.pos = position;
	info.rot = rotation;
	info.spawnerID = self->GetObjectID();

	auto* chest = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(chest);

	auto golemId = self->GetVar<LWOOBJID>(u"Golem");

	auto* golem = EntityManager::Instance()->GetEntity(golemId);

	if (golem != nullptr) {
		golem->Smash(self->GetObjectID());
	}
}

void FvMaelstromDragon::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	GameMessages::SendPlayFXEffect(self, -1, u"gothit", "", LWOOBJID_EMPTY, 1, 1, true);

	if (true) {
		auto weakpoint = self->GetVar<int32_t>(u"weakspot");

		if (weakpoint == 1) {
			self->Smash(attacker->GetObjectID());
		}
	}

	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();

	if (destroyableComponent != nullptr) {
		Game::logger->Log("FvMaelstromDragon", "Hit %i", destroyableComponent->GetArmor());

		if (destroyableComponent->GetArmor() > 0) return;

		auto weakpoint = self->GetVar<int32_t>(u"weakpoint");

		if (weakpoint == 0) {
			Game::logger->Log("FvMaelstromDragon", "Activating weakpoint");

			self->AddTimer("ReviveTimer", 12);

			auto* baseCombatAIComponent = self->GetComponent<BaseCombatAIComponent>();
			auto* skillComponent = self->GetComponent<SkillComponent>();

			if (baseCombatAIComponent != nullptr) {
				baseCombatAIComponent->SetDisabled(true);
				baseCombatAIComponent->SetStunned(true);
			}

			if (skillComponent != nullptr) {
				skillComponent->Interrupt();
			}

			self->SetVar<int32_t>(u"weakpoint", 2);

			GameMessages::SendPlayAnimation(self, u"stunstart", 1.7f);

			self->AddTimer("timeToStunLoop", 1);

			auto position = self->GetPosition();
			auto forward = self->GetRotation().GetForwardVector();
			auto backwards = forward * -1;

			forward.x *= 10;
			forward.z *= 10;

			auto rotation = self->GetRotation();

			auto objectPosition = NiPoint3();

			objectPosition.y = position.y;
			objectPosition.x = position.x - (backwards.x * 8);
			objectPosition.z = position.z - (backwards.z * 8);

			auto golem = self->GetVar<int32_t>(u"DragonSmashingGolem");

			EntityInfo info{};
			info.lot = golem != 0 ? golem : 8340;
			info.pos = objectPosition;
			info.rot = rotation;
			info.spawnerID = self->GetObjectID();
			info.settings = {
					new LDFData<std::string>(u"rebuild_activators",
						std::to_string(objectPosition.x + forward.x) + "\x1f" +
						std::to_string(objectPosition.y) + "\x1f" +
						std::to_string(objectPosition.z + forward.z)
					),
					new LDFData<int32_t>(u"respawn", 100000),
					new LDFData<float>(u"rebuild_reset_time", 15),
					new LDFData<bool>(u"no_timed_spawn", true),
					new LDFData<LWOOBJID>(u"Dragon", self->GetObjectID())
			};

			auto* golemObject = EntityManager::Instance()->CreateEntity(info);

			EntityManager::Instance()->ConstructEntity(golemObject);
		}
	}
}

void FvMaelstromDragon::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "ReviveHeldTimer") {
		self->AddTimer("backToAttack", 2.5);
	} else if (timerName == "ExposeWeakSpotTimer") {
		self->SetVar<int32_t>(u"weakspot", 1);
	} else if (timerName == "timeToStunLoop") {
		GameMessages::SendPlayAnimation(self, u"stunloop", 1.8f);
	} else if (timerName == "ReviveTimer") {
		GameMessages::SendPlayAnimation(self, u"stunend", 2.0f);
		self->AddTimer("backToAttack", 1);
	} else if (timerName == "backToAttack") {
		auto* baseCombatAIComponent = self->GetComponent<BaseCombatAIComponent>();
		auto* skillComponent = self->GetComponent<SkillComponent>();

		if (baseCombatAIComponent != nullptr) {
			baseCombatAIComponent->SetDisabled(false);
			baseCombatAIComponent->SetStunned(false);
		}

		if (skillComponent != nullptr) {
			skillComponent->Interrupt();
		}

		self->SetVar<int32_t>(u"weakspot", -1);

		GameMessages::SendNotifyObject(self->GetObjectID(), self->GetObjectID(), u"DragonRevive", UNASSIGNED_SYSTEM_ADDRESS);
	}
}

void
FvMaelstromDragon::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args != "rebuildDone") return;

	self->AddTimer("ExposeWeakSpotTimer", 3.8f);

	self->CancelTimer("ReviveTimer");

	self->AddTimer("ReviveHeldTimer", 10.5f);

	self->SetVar<LWOOBJID>(u"Golem", sender->GetObjectID());

	GameMessages::SendPlayAnimation(self, u"quickbuildhold", 1.9f);
}
