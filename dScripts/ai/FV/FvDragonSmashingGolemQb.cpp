#include "FvDragonSmashingGolemQb.h"
#include "GameMessages.h"
#include "EntityManager.h"

void FvDragonSmashingGolemQb::OnStartup(Entity* self) {
	self->AddTimer("GolemBreakTimer", 10.5f);
}

void FvDragonSmashingGolemQb::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "GolemBreakTimer") {
		self->Smash();
	}
}

void FvDragonSmashingGolemQb::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::REBUILD_COMPLETED) {
		GameMessages::SendPlayAnimation(self, u"dragonsmash");

		const auto dragonId = self->GetVar<LWOOBJID>(u"Dragon");

		auto* dragon = EntityManager::Instance()->GetEntity(dragonId);

		if (dragon != nullptr) {
			dragon->OnFireEventServerSide(self, "rebuildDone");
		}

		self->CancelTimer("GolemBreakTimer");
		self->AddTimer("GolemBreakTimer", 10.5f);
	}
}
