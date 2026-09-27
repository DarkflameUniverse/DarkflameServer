#include "WildAmbientCrab.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void WildAmbientCrab::OnStartup(Entity* self){
	self->SetVar(u"flipped", true);
	GameMessages::PlayAnimation(self->GetObjectID(), u"idle").Send(UNASSIGNED_SYSTEM_ADDRESS);
}

void WildAmbientCrab::OnUse(Entity* self, Entity* user) {
	auto flipped = self->GetVar<bool>(u"flipped");
	if (flipped) {
		self->AddTimer("Flipping", 0.6f);
		GameMessages::PlayAnimation(self->GetObjectID(), u"flip-over").Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar(u"flipped", false);
	} else if (!flipped) {
		self->AddTimer("Flipback", 0.8f);
		GameMessages::PlayAnimation(self->GetObjectID(), u"flip-back").Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar(u"flipped", true);
	}
}

void WildAmbientCrab::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "Flipping") GameMessages::PlayAnimation(self->GetObjectID(), u"over-idle").Send(UNASSIGNED_SYSTEM_ADDRESS);
    else if (timerName == "Flipback") GameMessages::PlayAnimation(self->GetObjectID(), u"idle").Send(UNASSIGNED_SYSTEM_ADDRESS);
}


