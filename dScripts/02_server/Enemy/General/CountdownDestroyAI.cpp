#include "CountdownDestroyAI.h"

#include "BaseCombatAIComponent.h"
#include "ScriptComponent.h"

void CountdownDestroyAI::OnStartup(Entity* self) {
	CountdownStartup(*self);
	auto* scriptComp = self->GetComponent<ScriptComponent>();
	if (scriptComp) scriptComp->RegisterMsg(this, &CountdownDestroyAI::OnNotifyCombatAIStateChange);
}

void CountdownDestroyAI::CountdownStartup(Entity& self) {
	auto suicideTimer = self.GetVar<float>(u"suicideTimer");
	if (suicideTimer == 0.0f) suicideTimer = 60;
	self.AddTimer("Dead", suicideTimer);
}

void CountdownDestroyAI::OnHit(Entity* self, Entity* attacker) {
	if (!self->GetVar<bool>(u"ShouldBeDead")) return;
	self->CancelTimer("IsBeingAttacked");
	self->AddTimer("Dead", 5.0f);
}

void CountdownDestroyAI::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "Dead") {
		self->SetVar<bool>(u"ShouldBeDead", true);
		if (self->GetVar<bool>(u"Busy")) {
			self->AddTimer("IsBeingAttacked", 5.0f);
		} else {
			self->Smash();
		}
	} else if (timerName == "IsBeingAttacked") {
		self->Smash();
	}
}

bool CountdownDestroyAI::OnNotifyCombatAIStateChange(Entity& self, GameMessages::NotifyCombatAIStateChange& notifyMsg) {
	const auto curState = notifyMsg.newState;
	if (curState == AiState::dead) return true;

	if (curState == AiState::aggro || curState == AiState::tether) {
		self.SetVar(u"Busy", true);
	} else {
		self.SetVar(u"Busy", false);
		if (self.GetVar<bool>(u"ShouldBeDead")) {
			self.Smash();
		}
	}
	return true;
}
