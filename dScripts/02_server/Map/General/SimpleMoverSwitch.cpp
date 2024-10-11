#include "SimpleMoverSwitch.h"
#include "GameMessages.h"

//	ensure switch_reset_time >= platform move time in hf

void SimpleMoverSwitch::OnStartup(Entity* self) {
	
//	Uses switch_reset_time from button	
	const auto SwitchEntity = Game::entityManager->GetEntitiesInGroup(self->GetVarAsString(u"switchGroup"));
	for (auto* button : SwitchEntity) {		
		self->SetVar<int32_t>(u"switch_reset_time", button->GetVar<int32_t>(u"switch_reset_time"));	
	}
	
	self->AddTimer("Reset", 19);
}	

void SimpleMoverSwitch::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, 
	int32_t param1, int32_t param2, int32_t param3) {
	if (self->GetVar<bool>(u"ReadyForUse") != true) return;
	
	if (args == "OnActivated") {	
			self->SetVar<bool>(u"ReadyForUse", false);				
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1);
			
			auto SwitchResetTime = self->GetVar<int32_t>(u"switch_reset_time");
			self->AddTimer("Reset", SwitchResetTime);
	}	
}

void SimpleMoverSwitch::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "Reset") {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0);
		
		auto SwitchResetTime = self->GetVar<int32_t>(u"switch_reset_time");		
		self->AddTimer("Unlock", SwitchResetTime);			
	} else if (timerName == "Unlock") {	
		self->SetVar<bool>(u"ReadyForUse", true);
	}
}	
