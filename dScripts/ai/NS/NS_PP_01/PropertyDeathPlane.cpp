#include "PropertyDeathPlane.h"
#include "Entity.h"
#include "GameMessages.h"
#include "EntityManager.h"

void PropertyDeathPlane::OnCollisionPhantom(Entity* self, Entity* target) {
	const auto teleportGroup = EntityManager::Instance()->GetEntitiesInGroup("Teleport");

	if (teleportGroup.size() == 0) {
		return;
	}

	auto* teleport = teleportGroup[0];

	GameMessages::SendTeleport(target->GetObjectID(), teleport->GetPosition(), teleport->GetRotation(), target->GetSystemAddress());
}
