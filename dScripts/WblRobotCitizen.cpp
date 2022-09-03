#include "WblRobotCitizen.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "dLogger.h"


void WblRobotCitizen::OnStartup(Entity* self) {
	auto movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatformComponent) movingPlatformComponent->StartPathing();
}

void WblRobotCitizen::OnUse(Entity* self, Entity* user) {
	auto movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatformComponent) movingPlatformComponent->StopPathing();
	auto face = NiQuaternion::LookAt(self->GetPosition(), user->GetPosition());
	self->SetRotation(face);
	GameMessages::SendPlayAnimation(self, u"wave");
	self->AddTimer("animation time", m_AnimationTime);
}

void WblRobotCitizen::OnTimerDone(Entity* self, std::string timerName) {
	auto movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();
	if (movingPlatformComponent) movingPlatformComponent->ContinuePathing();
}
