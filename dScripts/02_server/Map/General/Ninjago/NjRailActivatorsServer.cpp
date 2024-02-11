#include "NjRailActivatorsServer.h"
#include "QuickBuildComponent.h"
#include "Character.h"

void NjRailActivatorsServer::OnUse(Entity* self, Entity* user) {
	const auto flag = self->GetVar<int32_t>(u"RailFlagNum");
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	// Only allow use if this is not a quick build or the quick build is built
	if (quickBuildComponent == nullptr || quickBuildComponent->GetState() == eQuickBuildState::COMPLETED) {
		auto* character = user->GetCharacter();
		if (character != nullptr) {
			character->SetPlayerFlag(flag, true);
		}
	}
}
