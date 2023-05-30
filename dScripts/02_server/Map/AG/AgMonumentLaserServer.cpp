#include "AgMonumentLaserServer.h"
#include "EntityManager.h"

void AgMonumentLaserServer::OnStartup(Entity* self) {
	auto lasers = EntityManager::Instance()->GetEntitiesInGroup(self->GetVarAsString(u"volGroup"));
	for (auto laser : lasers) {
		if (laser) laser->SetBoolean(u"active", true);
	}
}

void AgMonumentLaserServer::OnDie(Entity* self, Entity* killer) {
	auto lasers = EntityManager::Instance()->GetEntitiesInGroup(self->GetVarAsString(u"volGroup"));
	for (auto laser : lasers) {
		if (laser) laser->SetBoolean(u"active", false);
	}
}

