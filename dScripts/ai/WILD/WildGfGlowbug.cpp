#include "WildGfGlowbug.h"
#include "GameMessages.h"

void WildGfGlowbug::OnStartup(Entity* self){
	self->SetVar(u"switch", false);
}

void WildGfGlowbug::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	if (args == "physicsReady") {
		auto switchState = self->GetVar<bool>(u"switch");
		if (!switchState) {
			GameMessages::SendStopFXEffect(self, true, "glowlight");
		} else if (switchState) {
			GameMessages::SendPlayFXEffect(self, -1, u"light", "glowlight", LWOOBJID_EMPTY);
		}
	}
}

void WildGfGlowbug::OnUse(Entity* self, Entity* user) {
	auto switchState = self->GetVar<bool>(u"switch");
	if (switchState) {
		GameMessages::SendStopFXEffect(self, true, "glowlight");
		self->SetVar(u"switch", false);
	} else if (!switchState) {
		GameMessages::SendPlayFXEffect(self, -1, u"light", "glowlight", LWOOBJID_EMPTY);
		self->SetVar(u"switch", true);
	}
}
