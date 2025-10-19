#include "GfBananaCluster.h"
#include "Entity.h"
#include "dpWorld.h"
#include "dNavMesh.h"
#include "Loot.h"
#include "DestroyableComponent.h"

void GfBananaCluster::OnStartup(Entity* self) {
	self->AddTimer("startup", 100);
}

void GfBananaCluster::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "startup") {
		self->ScheduleKillAfterUpdate(nullptr);
	}
}

// Hack in banana loot dropping from tree area since it seemed to do that in live for some reason
void GfBananaCluster::OnHit(Entity* self, Entity* attacker) {
	auto* parentEntity = self->GetParentEntity();
	GameMessages::GetPosition posMsg{};
	if (parentEntity) {
		posMsg.target = parentEntity->GetObjectID();
	}
	posMsg.Send();

	const auto rotation = parentEntity ? parentEntity->GetRotation() : self->GetRotation();

	if (dpWorld::GetNavMesh()) posMsg.pos.y = dpWorld::GetNavMesh()->GetHeightAtPoint(posMsg.pos) + 3.0f;
	else posMsg.pos = posMsg.pos - (NiPoint3Constant::UNIT_Y * 8);
	posMsg.pos.x -= QuatUtils::Right(rotation).x * 5;
	posMsg.pos.z -= QuatUtils::Right(rotation).z * 5;
	self->SetPosition(posMsg.pos);
}
