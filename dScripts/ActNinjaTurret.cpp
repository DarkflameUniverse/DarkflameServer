#include "ActNinjaTurret.h"

void ActNinjaTurret::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == eRebuildState::REBUILD_COMPLETED) {
		self->SetVar(u"AmBuilt", true);
	} else if (state == eRebuildState::REBUILD_RESETTING) {
		self->SetVar(u"AmBuilt", false);
	}
}

void
ActNinjaTurret::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	if (args == "ISpawned" && self->GetVar<bool>(u"AmBuilt")) {
		sender->Smash();
	}
}
