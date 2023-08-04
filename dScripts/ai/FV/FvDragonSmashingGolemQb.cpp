#include "FvDragonSmashingGolemQb.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "RenderComponent.h"
#include "Entity.h"
#include "eRebuildState.h"

void FvDragonSmashingGolemQb::OnStartup(Entity* self) {
	self->AddTimer("GolemBreakTimer", 10.5f);
}

void FvDragonSmashingGolemQb::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "GolemBreakTimer") {
		self->Smash();
	}
}

void FvDragonSmashingGolemQb::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::COMPLETED) {
		RenderComponent::PlayAnimation(self, u"dragonsmash");

		const auto dragonId = self->GetVar<LWOOBJID>(u"Dragon");

		auto* dragon = Game::entityManager->GetEntity(dragonId);

		if (dragon != nullptr) {
			dragon->OnFireEventServerSide(self, "rebuildDone");
		}

		self->CancelTimer("GolemBreakTimer");
		self->AddTimer("GolemBreakTimer", 10.5f);
	}
}
