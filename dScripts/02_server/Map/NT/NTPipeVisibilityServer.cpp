#include "NTPipeVisibilityServer.h"
#include "Entity.h"
#include "Character.h"

void NTPipeVisibilityServer::OnQuickBuildComplete(Entity* self, Entity* target) {
	const auto flag = self->GetVar<int32_t>(u"flag");
	if (flag == 0) return;

	auto* character = target->GetCharacter();
	if (!character) return;

	character->SetPlayerFlag(flag, true);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PipeBuilt");
}
