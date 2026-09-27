#include "WildGfGlowbug.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void WildGfGlowbug::OnStartup(Entity* self){
	self->SetVar(u"switch", false);
}

void WildGfGlowbug::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "physicsReady") {
		auto switchState = self->GetVar<bool>(u"switch");
		if (!switchState) {
			GameMessages::StopFXEffect(self->GetObjectID(), true, "glowlight").Send(UNASSIGNED_SYSTEM_ADDRESS);
		} else if (switchState) {
			GameMessages::PlayFXEffect(self->GetObjectID(), -1, u"light", "glowlight").Send(UNASSIGNED_SYSTEM_ADDRESS);
		}
	}
}

void WildGfGlowbug::OnUse(Entity* self, Entity* user) {
	auto switchState = self->GetVar<bool>(u"switch");
	if (switchState) {
		GameMessages::StopFXEffect(self->GetObjectID(), true, "glowlight").Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar(u"switch", false);
	} else if (!switchState) {
		GameMessages::PlayFXEffect(self->GetObjectID(), -1, u"light", "glowlight").Send(UNASSIGNED_SYSTEM_ADDRESS);
		self->SetVar(u"switch", true);
	}
}
