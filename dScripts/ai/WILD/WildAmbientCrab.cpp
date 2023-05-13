#include "WildAmbientCrab.h"
#include "GameMessages.h"

void WildAmbientCrab::OnStartup(Entity* self){
	self->SetVar(u"flipped", true);
	GameMessages::SendPlayAnimation(self, u"idle");
}

void WildAmbientCrab::OnUse(Entity* self, Entity* user) {
	auto flipped = self->GetVar<bool>(u"flipped");
	if (flipped) {
		self->AddTimer("Flipping", 0.6f);
		GameMessages::SendPlayAnimation(self, u"flip-over");
		self->SetVar(u"flipped", false);
	} else if (!flipped) {
		self->AddTimer("Flipback", 0.8f);
		GameMessages::SendPlayAnimation(self, u"flip-back");
		self->SetVar(u"flipped", true);
	}
}

void WildAmbientCrab::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "Flipping") GameMessages::SendPlayAnimation(self, u"over-idle");
    else if (timerName == "Flipback") GameMessages::SendPlayAnimation(self, u"idle");
}


