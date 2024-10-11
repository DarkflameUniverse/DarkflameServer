#include "NjFireRocks.h"
#include "ProximityMonitorComponent.h"
#include "EntityManager.h"
#include "DestroyableComponent.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"
#include "GameMessages.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void NjFireRocks::OnStartup(Entity* self) {
	
	if (self->GetVar<std::u16string>(u"RockManagerGroup") != u"LavaRocks02Controller") {
		self->SetVar<std::u16string>(u"RockManagerGroup", u"FireTransRocksManager");
	}
	
	self->AddTimer("StartRocks", 23);	
}	

void NjFireRocks::OnTimerDone(Entity* self, std::string timerName) {	

	if (timerName == "PlatHold") {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, 
		eMovementPlatformState::Moving);

		GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, 
		"{5433a544-758e-4e9e-80d7-e60799f5b7ff}");
		self->AddTimer("StopSFX", 2.7f);		
		
		self->AddTimer("StartRocks", 7);						
	}
	else if (timerName == "StartRocks") {		
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1, 
		eMovementPlatformState::Moving);
		
		GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, 
		"{5433a544-758e-4e9e-80d7-e60799f5b7ff}");	
		self->AddTimer("StopSFX", 2.7f);		
		
		self->AddTimer("PlatHold", 9.5f);				
	}
	else if (timerName == "StopSFX") {	
		GameMessages::SendStopNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, 
		"{5433a544-758e-4e9e-80d7-e60799f5b7ff}");
	}
}		
