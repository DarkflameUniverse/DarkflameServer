#include "AgQbElevator.h"
#include "EntityManager.h"
#include "GameMessages.h"

void AgQbElevator::OnStartup(Entity* self) {

}

//when the QB is finished being built by a player
void AgQbElevator::OnRebuildComplete(Entity* self, Entity* target) {
	self->SetProximityRadius(proxRadius, "elevatorProx");
	self->SetI64(u"qbPlayer", target->GetObjectID());

	float delayTime = killTime - endTime;
	if (delayTime < 1) delayTime = 1;

	GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
		0, 0, MovementPlatformState::Stationary);

	//add a timer that will kill the QB if no players get on in the killTime
	self->AddTimer("startKillTimer", killTime);
}

void AgQbElevator::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	//make sure we haven't already started pathing.
	if (self->GetBoolean(u"qbPlayerRdy")) return;

	if (status == "ENTER") {
		Entity* builder = EntityManager::Instance()->GetEntity(self->GetI64(u"qbPlayer"));
		if (builder && builder == entering) {
			//the builder has entered so cancel the start timer and just start moving
			self->SetBoolean(u"qbPlayerRdy", true);
			self->CancelTimer("StartElevator");

			GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
				1, 1, MovementPlatformState::Moving);
		} else if (!self->GetBoolean(u"StartTimer")) {
			self->SetBoolean(u"StartTimer", true);
			self->AddTimer("StartElevator", startTime);
		}
	}
}

void AgQbElevator::OnTimerDone(Entity* self, std::string timerName) {

	if (timerName == "StartElevator") {
		GameMessages::SendPlatformResync(self, UNASSIGNED_SYSTEM_ADDRESS, true, 0,
			1, 1, MovementPlatformState::Moving);
	} else if (timerName == "startKillTimer") {
		killTimerStartup(self);
	} else if (timerName == "KillTimer") {
		self->Smash(self->GetObjectID(), VIOLENT);
	}
}

void AgQbElevator::killTimerStartup(Entity* self) const {
	self->CancelAllTimers();
	self->AddTimer("KillTimer", endTime);
	self->SetNetworkVar<float>(u"startEffect", endTime); // Blinking effect
}
