#include "BaseEnemyApe.h"
#include "BaseCombatAIComponent.h"
#include "DestroyableComponent.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "SkillComponent.h"

void BaseEnemyApe::OnStartup(Entity* self) {
	self->SetVar<uint32_t>(u"timesStunned", 2);
	self->SetVar<bool>(u"knockedOut", false);
}

void BaseEnemyApe::OnDie(Entity* self, Entity* killer) {
	auto* anchor = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"QB"));
	if (anchor != nullptr && !anchor->GetIsDead()) {
		anchor->Smash(self->GetObjectID(), SILENT);
	}
}

void BaseEnemyApe::OnSkillCast(Entity* self, uint32_t skillID) {
	const auto groundPoundSkill = self->GetVar<uint32_t>(u"GroundPoundSkill") != 0 ? self->GetVar<uint32_t>(u"GroundPoundSkill") : 725;
	const auto spawnQuickBuildTime = self->GetVar<float_t>(u"spawnQBTime") != 0.0f ? self->GetVar<float_t>(u"spawnQBTime") : 5.0f;

	if (skillID == groundPoundSkill && self->GetVar<LWOOBJID>(u"QB") == LWOOBJID_EMPTY) {
		self->AddTimer("spawnQBTime", spawnQuickBuildTime);
	}
}

void BaseEnemyApe::OnHit(Entity* self, Entity* attacker) {
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	if (destroyableComponent != nullptr && destroyableComponent->GetArmor() < 1 && !self->GetBoolean(u"knockedOut")) {
		StunApe(self, true);
		self->CancelTimer("spawnQBTime");

		GameMessages::SendPlayAnimation(self, u"disable", 1.7f);

		const auto reviveTime = self->GetVar<float_t>(u"reviveTime") != 0.0f
			? self->GetVar<float_t>(u"reviveTime") : 12.0f;
		self->AddTimer("reviveTime", reviveTime);
	}
}

void BaseEnemyApe::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "reviveTime") {

		// Revives the ape, giving it back some armor
		const auto timesStunned = self->GetVar<uint32_t>(u"timesStunned");
		auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
		if (destroyableComponent != nullptr) {
			destroyableComponent->SetArmor(destroyableComponent->GetMaxArmor() / timesStunned);
		}
		EntityManager::Instance()->SerializeEntity(self);
		self->SetVar<uint32_t>(u"timesStunned", timesStunned + 1);
		StunApe(self, false);

	} else if (timerName == "spawnQBTime" && self->GetVar<LWOOBJID>(u"QB") == LWOOBJID_EMPTY) {
		// Spawn QB in front of ape.
		const auto position = self->GetPosition();
		const auto rotation = self->GetRotation();

		const auto backwardVector = rotation.GetForwardVector() * -1;
		const auto objectPosition = NiPoint3(
			position.GetX() - (backwardVector.GetX() * 8),
			position.GetY(),
			position.GetZ() - (backwardVector.GetZ() * 8)
		);

		EntityInfo entityInfo{};

		entityInfo.pos = position;
		entityInfo.rot = rotation;
		entityInfo.pos.SetY(entityInfo.pos.GetY() + 13.0f);

		entityInfo.spawnerID = self->GetObjectID();
		entityInfo.lot = self->GetVar<LOT>(u"QuickbuildAnchorLOT") != 0
			? self->GetVar<LOT>(u"QuickbuildAnchorLOT") : 7549;
		entityInfo.settings = {
			new LDFData<std::string>(u"rebuild_activators",
									 std::to_string(objectPosition.GetX()) + "\x1f" +
									 std::to_string(objectPosition.GetY()) + "\x1f" +
									 std::to_string(objectPosition.GetZ())
			),
			new LDFData<bool>(u"no_timed_spawn", true),
			new LDFData<LWOOBJID>(u"ape", self->GetObjectID())
		};

		auto* anchor = EntityManager::Instance()->CreateEntity(entityInfo);
		EntityManager::Instance()->ConstructEntity(anchor);
		self->SetVar<LWOOBJID>(u"QB", anchor->GetObjectID());

	} else if (timerName == "anchorDamageTimer") {

		// Attacks the ape with some god skill
		const auto* player = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"smasher"));
		if (player == nullptr) {
			return;
		}

		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (skillComponent != nullptr) {
			skillComponent->CalculateBehavior(1273, 29446, self->GetObjectID(), true, false, player->GetObjectID());
		}

		self->SetVar<LWOOBJID>(u"QB", LWOOBJID_EMPTY);
	}
}

void BaseEnemyApe::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "rebuildDone" && sender != nullptr) {
		self->SetVar<LWOOBJID>(u"smasher", sender->GetObjectID());
		const auto anchorDamageDelayTime = self->GetVar<float_t>(u"AnchorDamageDelayTime") != 0.0f ? self->GetVar<float_t>(u"AnchorDamageDelayTime") : 0.5f;
		self->AddTimer("anchorDamageTimer", anchorDamageDelayTime);
	}
}

void BaseEnemyApe::StunApe(Entity* self, bool stunState) {
	auto* combatAIComponent = self->GetComponent<BaseCombatAIComponent>();
	if (combatAIComponent != nullptr) {
		combatAIComponent->SetDisabled(stunState);
		combatAIComponent->SetStunned(stunState);

		auto* skillComponent = self->GetComponent<SkillComponent>();
		if (skillComponent != nullptr) {
			skillComponent->Interrupt();
		}

		GameMessages::SendSetStunned(self->GetObjectID(), stunState ? PUSH : POP, UNASSIGNED_SYSTEM_ADDRESS, self->GetObjectID(),
			true, true, true, true, true,
			true, true, true, true);

		self->SetBoolean(u"knockedOut", stunState);
	}
}
