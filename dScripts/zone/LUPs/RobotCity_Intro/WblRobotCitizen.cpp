#include "WblRobotCitizen.h"
#include "MovementAIComponent.h"
#include "RenderComponent.h"

void WblRobotCitizen::OnStartup(Entity* self) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
}

void WblRobotCitizen::OnUse(Entity* self, Entity* user) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (movementAIComponent) movementAIComponent->Pause();
	auto face = NiQuaternion::LookAt(self->GetPosition(), user->GetPosition());
	self->SetRotation(face);
	auto timer = RenderComponent::PlayAnimation(self, "wave", 0.4f);
	self->AddTimer("animation time", timer);
}

void WblRobotCitizen::OnTimerDone(Entity* self, std::string timerName) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
	movementAIComponent->Resume();
}
