#include "NtImagimeterVisibility.h"
#include "GameMessages.h"
#include "Entity.h"
#include "ePlayerFlag.h"

void NTImagimeterVisibility::OnQuickBuildComplete(Entity* self, Entity* target) {
	GameMessages::SetFlag setFlag{};
	setFlag.target = target->GetObjectID();
	setFlag.iFlagId = ePlayerFlag::NT_PLINTH_REBUILD;
	setFlag.bFlag = true;
	SEND_ENTITY_MSG(setFlag);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlinthBuilt", 0, 0, LWOOBJID_EMPTY, "", target->GetSystemAddress());
}
