#include "CatapultBouncerServer.h"
#include "GameMessages.h"
#include "ObjectMessages.h"
#include "EntityManager.h"

void CatapultBouncerServer::OnQuickBuildComplete(Entity* self, Entity* target) {
	GameMessages::NotifyClientObject(self->GetObjectID(), u"Built", 0, 0, LWOOBJID_EMPTY, "").Send(UNASSIGNED_SYSTEM_ADDRESS);

	self->SetNetworkVar<bool>(u"Built", true);

	const auto base = Game::entityManager->GetEntitiesInGroup(self->GetVarAsString(u"BaseGroup"));

	for (auto* obj : base) {
		obj->NotifyObject(self, "BouncerBuilt");
	}
}
