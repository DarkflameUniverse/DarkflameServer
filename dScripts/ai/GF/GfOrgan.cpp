#include "GfOrgan.h"
#include "GameMessages.h"
#include "Entity.h"
#include "RenderComponent.h"

void GfOrgan::OnUse(Entity* self, Entity* user) {
	if (self->GetBoolean(u"bIsInUse")) {
		m_canUse = false;
		return;
	}

	GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, "{15d5f8bd-139a-4c31-8904-970c480cd70f}");
	self->SetBoolean(u"bIsInUse", true);
	self->AddTimer("reset", 5.0f);

	RenderComponent::PlayAnimation(user, u"jig");
}

void GfOrgan::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "reset") {
		self->SetBoolean(u"bIsInUse", false);
	}
}
