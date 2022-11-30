#include "ActMine.h"
#include "SkillComponent.h"
#include "DestroyableComponent.h"
#include "RebuildComponent.h"

void ActMine::OnStartup(Entity* self) {
	self->SetVar(u"RebuildComplete", false);
	self->SetProximityRadius(MINE_RADIUS, "mineRadius");
}

void ActMine::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::REBUILD_COMPLETED) {
		auto* rebuild = self->GetComponent<RebuildComponent>();
		if (rebuild) {
			auto* builder = rebuild->GetBuilder();
			self->SetVar(u"Builder", builder->GetObjectID());
		}

		self->SetVar(u"RebuildComplete", true);
		self->SetVar(u"NumWarnings", 0);
		self->AddToGroup("reset");
	}

}

void ActMine::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* detroyable = self->GetComponent<DestroyableComponent>();
	if (!detroyable) return;
	if (status == "ENTER" && self->GetVar<bool>(u"RebuildComplete") == true && detroyable->IsEnemy(entering)) {
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 242, u"orange", "sirenlight_B");
		self->AddTimer("Tick", TICK_TIME);
	}
}

void ActMine::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "Tick") {
		if (self->GetVar<int>(u"NumWarnings") >= MAX_WARNINGS) {
			auto* skill = self->GetComponent<SkillComponent>();
			if (!skill) return;
			skill->CalculateBehavior(SKILL_ID, BEHAVIOR_ID, LWOOBJID_EMPTY);
			self->AddTimer("BlowedUp", BLOWED_UP_TIME);
		} else {
			GameMessages::SendPlayFXEffect(self->GetObjectID(), 242, u"orange", "sirenlight_B");
			self->AddTimer("Tick", TICK_TIME);
			self->SetVar(u"NumWarnings", self->GetVar<int>(u"NumWarnings") + 1);
		}
	}

	if (timerName == "BlowedUp") {
		self->Kill(self);
	}
}
