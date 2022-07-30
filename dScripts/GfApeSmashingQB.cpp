#include "GfApeSmashingQB.h"
#include "EntityManager.h"
#include "GameMessages.h"

void GfApeSmashingQB::OnStartup(Entity* self) {
	self->SetNetworkVar<LWOOBJID>(u"lootTagOwner", self->GetVar<LWOOBJID>(u"lootTagOwner"));
}

void GfApeSmashingQB::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "anchorBreakTime") {
		self->Smash();
	}
}

void GfApeSmashingQB::OnRebuildComplete(Entity* self, Entity* target) {
	auto* ape = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"ape"));
	if (ape != nullptr) {
		ape->OnFireEventServerSide(target, "rebuildDone");
		GameMessages::SendPlayAnimation(self, u"smash", 1.7f);
		self->AddTimer("anchorBreakTime", 1.0f);
	}
}
