#include "NTPipeVisibilityServer.h"

#include "Entity.h"

void NTPipeVisibilityServer::OnQuickBuildComplete(Entity* self, Entity* target) {
	const auto flag = self->GetVar<int32_t>(u"flag");
	if (flag == 0) return;
	GameMessages::SetFlag setFlag{};
	setFlag.target = target->GetObjectID();
	setFlag.iFlagId = flag;
	setFlag.bFlag = true;
	SEND_ENTITY_MSG(setFlag);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PipeBuilt");
}
