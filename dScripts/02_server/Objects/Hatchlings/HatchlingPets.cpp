#include "HatchlingPets.h"

#include "Entity.h"
#include "MovementAIComponent.h"

void HatchlingPets::OnStartup(Entity* self) {
	self->SetVar(u"follow", false);

	self->SetProximityRadius(5, "StopFollow");
	self->SetProximityRadius(15, "Wander");
	self->SetProximityRadius(50, "Teleport");

	Wander(*self, *self->GetOwner());
	self->AddComponent<MovementAIComponent>(-1, MovementAIInfo{ .wanderRadius = 2.5f });
}

void HatchlingPets::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	auto* const parent = self->GetOwner();
	if (!entering || !entering->IsPlayer() || parent->GetObjectID() != entering->GetObjectID()) return;

	if (name == "StopFollow") {
		if (status == "ENTER") {
			if (self->GetVar<bool>(u"follow")) {
				const auto randomWanderTime = GeneralUtils::GenerateRandomNumber<float>(4, 9);
				self->AddTimer("StartWander", randomWanderTime);
				// stop following the player
				auto* const movementAI = self->GetComponent<MovementAIComponent>();
				if (movementAI) {
					movementAI->Stop();
					movementAI->FollowTarget(LWOOBJID_EMPTY);
				}
				self->SetVar(u"follow", false);
			}
		}
	} else if (name == "Wander") {
		if (status == "LEAVE") {
			self->CancelAllTimers();
			// follow the player
			auto* const movementAI = self->GetComponent<MovementAIComponent>();
			if (movementAI) {
				movementAI->Stop();
				movementAI->FollowTarget(entering->GetObjectID());
			}
			self->SetVar(u"follow", true);
		}
	} else if (name == "Teleport") {
		if (status == "LEAVE") {
			// stop following the player
			auto* const movementAI = self->GetComponent<MovementAIComponent>();
			if (movementAI) {
				movementAI->Stop();
				movementAI->FollowTarget(LWOOBJID_EMPTY);
			}
			GameMessages::GetPosition getPos;
			getPos.Send(entering->GetObjectID());
			getPos.pos.z += 5.0f;
			self->SetPosition(getPos.pos);
			Game::entityManager->SerializeEntity(*self);
		}
	}
}

void HatchlingPets::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "StartWander") {
		Wander(*self, *self->GetOwner());
	}
}

void HatchlingPets::Wander(Entity& self, Entity& player) {
	GameMessages::GetPosition getPos;
	if (!getPos.Send(player.GetObjectID())) {
		LOG("Failed to get position for %llu", player.GetObjectID());
		return;
	}

	const auto xWander = GeneralUtils::GenerateRandomNumber<float>(0, 20) - 10.0f;
	const auto zWander = GeneralUtils::GenerateRandomNumber<float>(0, 20) - 10.0f;
	getPos.pos.x += xWander;
	getPos.pos.z += zWander;
	auto* const movementAI = self.GetComponent<MovementAIComponent>();
	if (movementAI) movementAI->SetDestination(getPos.pos);
	self.AddTimer("StartWander", GeneralUtils::GenerateRandomNumber<float>(4, 9));
}
