#include "WildNinjaBricks.h"
#include "Entity.h"

void WildNinjaBricks::OnStartup(Entity* self) {
	self->AddToGroup("Ninjastuff");
}

void WildNinjaBricks::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "Crane") GameMessages::SendPlayAnimation(self, u"crane");
	else if (name == "Tiger") GameMessages::SendPlayAnimation(self, u"tiger");
	else if (name == "Mantis") GameMessages::SendPlayAnimation(self, u"mantis");
}

