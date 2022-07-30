#include "ImaginationShrineServer.h"
#include "RebuildComponent.h"

void ImaginationShrineServer::OnUse(Entity* self, Entity* user) {
	// If the rebuild component is complete, use the shrine
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();

	if (rebuildComponent == nullptr) {
		return;
	}

	if (rebuildComponent->GetState() == REBUILD_COMPLETED) {
		// Use the shrine
		BaseUse(self, user);
	}
}
