#include "NjRailPostServer.h"
#include "RebuildComponent.h"
#include "EntityManager.h"

void NjRailPostServer::OnStartup(Entity* self) {
	auto* rebuildComponent = self->GetComponent<RebuildComponent>();
	if (rebuildComponent != nullptr) {
		self->SetNetworkVar<bool>(NetworkNotActiveVariable, true);
	}
}

void NjRailPostServer::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1,
	int32_t param2) {
	if (name == "PostRebuilt") {
		self->SetNetworkVar<bool>(NetworkNotActiveVariable, false);
	} else if (name == "PostDied") {
		self->SetNetworkVar<bool>(NetworkNotActiveVariable, true);
	}
}

void NjRailPostServer::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == REBUILD_COMPLETED) {
		auto* relatedRail = GetRelatedRail(self);
		if (relatedRail == nullptr)
			return;

		relatedRail->NotifyObject(self, "PostRebuilt");

		if (self->GetVar<bool>(NotActiveVariable))
			return;

		self->SetNetworkVar(NetworkNotActiveVariable, false);
	} else if (state == REBUILD_RESETTING) {
		auto* relatedRail = GetRelatedRail(self);
		if (relatedRail == nullptr)
			return;

		relatedRail->NotifyObject(self, "PostDied");
	}
}

Entity* NjRailPostServer::GetRelatedRail(Entity* self) {
	const auto& railGroup = self->GetVar<std::u16string>(RailGroupVariable);
	if (!railGroup.empty()) {
		for (auto* entity : EntityManager::Instance()->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(railGroup))) {
			return entity;
		}
	}

	return nullptr;
}
