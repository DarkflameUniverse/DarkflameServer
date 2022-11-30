#include "NtXRayServer.h"
#include "SkillComponent.h"

void NtXRayServer::OnCollisionPhantom(Entity* self, Entity* target) {
	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(1220, 27641, target->GetObjectID());
}
