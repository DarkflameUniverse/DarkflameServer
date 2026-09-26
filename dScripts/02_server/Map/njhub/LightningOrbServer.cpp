#include "LightningOrbServer.h"

void LightningOrbServer::OnCollisionPhantom(Entity* self, Entity* target) {
	GameMessages::GetPosition playerPos;
	playerPos.Send(target->GetObjectID());
	GameMessages::GetPosition selfPos;
	selfPos.Send(self->GetObjectID());
	const NiPoint3 newVec((playerPos.pos.x - selfPos.pos.x) * 2.5, 15, (playerPos.pos.z - selfPos.pos.z) * 2.5);
	// ahhhh aron said to put a TODO here moving platforms don't work lol.  disable this so people can actually do the puzzle
	// GameMessages::SendKnockback(target->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 0, newVec);
	// GameMessages::SendPlayFXEffect(target->GetObjectID(), -1, u"knockback", "knockback");
}
