#include "WblRobotCitizen.h"
#include "GameMessages.h"

void WblRobotCitizen::OnStartup(Entity* self) {
	// TODO: make it move via controllable physics
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
	// TODO: make it move via controllable physics
}
