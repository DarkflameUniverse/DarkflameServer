#include "WildNinjaStudent.h"
#include "GameMessages.h"

void WildNinjaStudent::OnStartup(Entity* self) {
	self->AddToGroup("Ninjastuff");
	GameMessages::SendPlayAnimation(self, u"bow");
}

void WildNinjaStudent::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "Crane") GameMessages::SendPlayAnimation(self, u"crane");
	else if (name == "Tiger") GameMessages::SendPlayAnimation(self, u"tiger");
	else if (name == "Mantis") GameMessages::SendPlayAnimation(self, u"mantis");
	else if (name == "Bow") GameMessages::SendPlayAnimation(self, u"bow");
}
