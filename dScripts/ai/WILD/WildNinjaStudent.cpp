#include "WildNinjaStudent.h"
#include "GameMessages.h"
#include "EffectsMessages.h"

void WildNinjaStudent::OnStartup(Entity* self) {
	self->AddToGroup("Ninjastuff");
	GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
}

void WildNinjaStudent::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "Crane") GameMessages::PlayAnimation(self->GetObjectID(), u"crane").Send(UNASSIGNED_SYSTEM_ADDRESS);
	else if (name == "Tiger") GameMessages::PlayAnimation(self->GetObjectID(), u"tiger").Send(UNASSIGNED_SYSTEM_ADDRESS);
	else if (name == "Mantis") GameMessages::PlayAnimation(self->GetObjectID(), u"mantis").Send(UNASSIGNED_SYSTEM_ADDRESS);
	else if (name == "Bow") GameMessages::PlayAnimation(self->GetObjectID(), u"bow").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
