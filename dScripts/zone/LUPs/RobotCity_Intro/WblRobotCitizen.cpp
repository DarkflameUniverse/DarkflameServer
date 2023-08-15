#include "WblRobotCitizen.h"
#include "MovementAIComponent.h"
#include "RenderComponent.h"

void WblRobotCitizen::OnStartup(Entity* self) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
	movementAIComponent->Resume();
}

void WblRobotCitizen::OnUse(Entity* self, Entity* user) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
	movementAIComponent->Pause();
	auto face = NiQuaternion::LookAt(self->GetPosition(), user->GetPosition());
	self->SetRotation(face);
	auto timer = RenderComponent::PlayAnimation(self, "wave");
	self->AddTimer("animation time", timer);
}

void WblRobotCitizen::OnTimerDone(Entity* self, std::string timerName) {
	auto movementAIComponent = self->GetComponent<MovementAIComponent>();
	if (!movementAIComponent) return;
	movementAIComponent->Resume();
}
