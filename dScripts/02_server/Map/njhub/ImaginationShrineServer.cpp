#include "ImaginationShrineServer.h"
#include "QuickBuildComponent.h"

void ImaginationShrineServer::OnUse(Entity* self, Entity* user) {
	// If the rebuild component is complete, use the shrine
	auto* quickBuildComponent = self->GetComponent<QuickBuildComponent>();

	if (quickBuildComponent == nullptr) {
		return;
	}

	if (quickBuildComponent->GetState() == eRebuildState::COMPLETED) {
		// Use the shrine
		BaseUse(self, user);
	}
}
