#include "NTPipeVisibilityServer.h"
#include "Entity.h"
#include "Character.h"
#include "ObjectMessages.h"

void NTPipeVisibilityServer::OnQuickBuildComplete(Entity* self, Entity* target) {
	const auto flag = self->GetVar<int32_t>(u"flag");
	if (flag == 0) return;

	auto* character = target->GetCharacter();
	if (!character) return;

	character->SetPlayerFlag(flag, true);

	GameMessages::NotifyClientObject(self->GetObjectID(), u"PipeBuilt").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
