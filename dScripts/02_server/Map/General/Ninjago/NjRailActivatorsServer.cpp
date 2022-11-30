#include "NjRailActivatorsServer.h"
#include "RebuildComponent.h"
#include "Character.h"

void NjRailActivatorsServer::OnUse(Entity* self, Entity* user) {
	const auto flag = self->GetVar<int32_t>(u"RailFlagNum");
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	// Only allow use if this is not a quick build or the quick build is built
	if (rebuildComponent == nullptr || rebuildComponent->GetState() == REBUILD_COMPLETED) {
		auto* character = user->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(flag, true);
		}
	}
}
