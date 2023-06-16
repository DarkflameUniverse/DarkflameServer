#include "WildNinjaBricks.h"
#include "Entity.h"

void WildNinjaBricks::OnStartup(Entity* self) {
	self->AddToGroups("Ninjastuff");
}

void WildNinjaBricks::OnNotifyObject(Entity* self, Entity* sender, const std::u16string& name, int32_t param1, int32_t param2) {
	if (name == u"Crane") GameMessages::SendPlayAnimation(self, u"crane");
	else if (name == u"Tiger") GameMessages::SendPlayAnimation(self, u"tiger");
	else if (name == u"Mantis") GameMessages::SendPlayAnimation(self, u"mantis");
}

