//	For fire chest puzzle in 8spinners

#include "Button70.h"
#include "Entity.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "EntityInfo.h"

void Button70::OnStartup(Entity* self) {
	IsInMovement = 0;
}

void Button70::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	
	if (args == "OnActivated") {
		if (IsInMovement == 0) {		
			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);
			self->AddTimer("MoveBack", 5.0f);	
		}	

		IsInMovement = 1;
	} 
}

void Button70::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, 
int32_t param1, int32_t param2) {
	
	if (name == "OnChestActivated") {		
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);	
	}
}	

void Button70::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "MoveBack") {	
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0, 1, 1);	
		self->AddTimer("IsBack", 3.0f);		
	}
	if (timerName == "IsBack") {		
		IsInMovement = 0;	
	}
}
