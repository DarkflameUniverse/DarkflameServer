#include "Turret.h"

#include "BaseCombatAIComponent.h"
#include "ControllablePhysicsComponent.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "eQuickBuildState.h"
#include "eStateChangeType.h"

void Turret::SetCombatAI(Entity* self, bool enabled) {
	if (auto* combatAI = self->GetComponent<BaseCombatAIComponent>()) combatAI->SetDisabled(!enabled);
}

void Turret::OnStartup(Entity* self) {
	SetCombatAI(self, false);
	self->SetVar<int32_t>(u"currentTime", 1);
	self->SetVar<bool>(u"building", false);
	self->AddTimer("TickTime", 1.0f);

	// SetStunImmunity (attack, interrupt) and SetStatusImmunity (pull to point, knockback); DLU keeps interrupt with the status ones
	if (auto* controllablePhysics = self->GetComponent<ControllablePhysicsComponent>()) {
		controllablePhysics->SetStunImmunity(eStateChangeType::PUSH, self->GetObjectID(), true, false, false, false, false, false, false);
	}
	if (auto* destroyable = self->GetComponent<DestroyableComponent>()) {
		destroyable->SetStatusImmunity(eStateChangeType::PUSH, false, false, true, true, false, false, false, false, true);
	}
}

void Turret::OnQuickBuildStart(Entity* self, Entity* target) {
	self->SetVar<bool>(u"building", true);
}

void Turret::OnQuickBuildNotifyState(Entity* self, eQuickBuildState state) {
	if (state != eQuickBuildState::COMPLETED) return;
	SetCombatAI(self, true);
	self->SetVar<bool>(u"building", false);
	self->CancelAllTimers();
	self->AddTimer("TickTime", 1.0f);
}

void Turret::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName != "TickTime") return;
	const auto currentTime = self->GetVar<int32_t>(u"currentTime") + 1;
	self->SetVar<int32_t>(u"currentTime", currentTime);
	if (currentTime >= KILL_TIME && !self->GetVar<bool>(u"building")) {
		SetCombatAI(self, false);
		self->SetVar<int32_t>(u"currentTime", 0);
		self->Smash(self->GetObjectID());
		return;
	}
	self->AddTimer("TickTime", 1.0f);
}
