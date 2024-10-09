// waypoints 0, 14, 24, 34, 40, 49, 56, 64, 72, 79

#include "FtGarmadonTornado.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "ProximityMonitorComponent.h"
#include "SkillComponent.h"
#include "EntityInfo.h"
#include "RenderComponent.h"

Entity* FtGarmadonTornado::randomPlayer = nullptr;

void FtGarmadonTornado::OnStartup(Entity* self) {	

	auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatform == nullptr) {
		return;
	}	
	
	const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");	
	for (auto* manager : BossManager) {	
		randomNum = manager->GetVar<int>(u"randNum");	
	}	
		
	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);

	
	self->AddTimer("StartPathing", 25);				
	self->AddTimer("FXLoop", 5);	
}


void FtGarmadonTornado::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, 
int32_t param1, int32_t param2) {
	
	
	
	if (name == "FindAPath") {
		self->AddTimer("BombPlayer", 28);	
		
	} else if (name == "StopPathing") {	
		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();	
		
		self->CancelTimer("BombPlayer");
		

	} else if (name == "PlaySummonAnim") {
		RenderComponent::PlayAnimation(self, u"attack");	
		
	} else if (name == "PlayerSent") {		
		
		RenderComponent::PlayAnimation(self, u"attack");	

	}
}	

void FtGarmadonTornado::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
	int nextWaypoint = waypointIndex + 1;



}	

	
	
//	I guess since moving platforms technically aren't moving, proximity doesn't move with moving plat

//	Functions to use if fixed, use "TornadoProx" proximity (instead of a phantom physics object)
// 	to avoid sending changes to clients
	
void FtGarmadonTornado::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (name == "TornadoProx") {
		if (entering->IsPlayer() && status == "ENTER") {
//			OnHit skill for player			
			auto* skillComponent = self->GetComponent<SkillComponent>();				
			skillComponent->CastSkill(this->TornadoHitSkill, entering->GetObjectID());
			
//			Knockback for player			
			auto dir = entering->GetRotation().GetForwardVector();
			dir.y = 18;
			dir.x = -dir.x * 21;
			dir.z = -dir.z * 21;
			GameMessages::SendKnockback(entering->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
		}
	}
}

void FtGarmadonTornado::OnCollisionPhantom(Entity* self, Entity* target) {	
//	if (target->IsPlayer()) {
//		OnHit skill for player			
//		auto* skillComponent = self->GetComponent<SkillComponent>();				
//		skillComponent->CastSkill(this->TornadoHitSkill, target->GetObjectID());
			
//		Knockback for player			
//		auto dir = target->GetRotation().GetForwardVector();
//		dir.y = 18;
//		dir.x = -dir.x * 21;
//		dir.z = -dir.z * 21;
//		GameMessages::SendKnockback(target->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);		
		
//	}	
}	

//	End of proximity/phantom functions



void FtGarmadonTornado::OnTimerDone(Entity* self, std::string timerName) {
	
	if (timerName == "BombPlayer") {	
	
//		Request random player from manager	
		const auto BossManager = Game::entityManager->GetEntitiesInGroup("BossManager");	
		for (auto* manager : BossManager) {	
			manager->NotifyObject(self, "RequestPlayer");
		}

		self->AddTimer("BombPlayer", 28);		

	}	
	if (timerName == "FXLoop") {	
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 32169, u"create", "garmadon_idle_mid");	
		GameMessages::SendPlayFXEffect(self->GetObjectID(), 32101, u"create", "garmadon_idle_base");
		
		
//		Make sure tornado @ proper waypoint		
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 1, 0, 0, eMovementPlatformState::Moving);

	}
	if (timerName == "StartPathing") {

		auto* movingPlatform = self->GetComponent<MovingPlatformComponent>();
		if (movingPlatform == nullptr) {
			return;
		}
			
					
		int waypoints[] = {0, 14, 24, 34, 40, 49, 56, 64, 72, 79};
		int scaledNum = randomNum + (std::rand() % 5);		
		int waypoint = waypoints[scaledNum];
		

		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, false, waypoint, waypoint + 1);
		
		
		self->AddTimer("StartPathing", 270.5f);
	}	
}


