#include "WildNinjaStudent.h"
#include "GameMessages.h"
#include "Entity.h"

void WildNinjaStudent::OnStartup(Entity* self) {
	self->AddToGroups("Ninjastuff");
	GameMessages::SendPlayAnimation(self, u"bow");
}

void WildNinjaStudent::OnNotifyObject(Entity* self, Entity* sender, const std::u16string& name, int32_t param1, int32_t param2) {
	if (name == u"Crane") GameMessages::SendPlayAnimation(self, u"crane");
	else if (name == u"Tiger") GameMessages::SendPlayAnimation(self, u"tiger");
	else if (name == u"Mantis") GameMessages::SendPlayAnimation(self, u"mantis");
	else if (name == u"Bow") GameMessages::SendPlayAnimation(self, u"bow");
}
