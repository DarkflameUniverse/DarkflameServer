#include "DoorIndicatorServer.h"
#include "Entity.h"

void DoorIndicatorServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2,
	int32_t param3) {
	
	const auto ActivatedEvent = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"name_activated_event"));
	const auto DeactivatedEvent = GeneralUtils::UTF16ToWTF8(self->GetVar<std::u16string>(u"name_deactivated_event"));
	
	if (args == ActivatedEvent) {
		self->SetNetworkVar<bool>(u"FlameOn", true);	
	} else if (args == DeactivatedEvent) {
		self->SetNetworkVar<bool>(u"FlameOn", true);
	}
}
