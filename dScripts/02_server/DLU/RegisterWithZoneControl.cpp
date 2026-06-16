#include "RegisterWithZoneControl.h"

#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"

void RegisterWithZoneControl::OnStartup(Entity* self) {
	GameMessages::ObjectLoaded objLoaded;
	objLoaded.objectID = self->GetObjectID();
	objLoaded.lot = self->GetLOT();
	objLoaded.Send(Game::entityManager->GetZoneControlEntity()->GetObjectID());
}
