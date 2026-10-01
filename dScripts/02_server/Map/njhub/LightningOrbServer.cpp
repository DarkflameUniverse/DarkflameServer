#include "LightningOrbServer.h"
#include "CombatMessages.h"
#include "EffectsMessages.h"

void LightningOrbServer::OnCollisionPhantom(Entity* self, Entity* target) {
	GameMessages::GetPosition playerPos;
	playerPos.Send(target->GetObjectID());
	GameMessages::GetPosition selfPos;
	selfPos.Send(self->GetObjectID());
	const NiPoint3 newVec((playerPos.pos.x - selfPos.pos.x) * 2.5, 15, (playerPos.pos.z - selfPos.pos.z) * 2.5);
	// L_LIGHTNING_ORB_SERVER (issue 1352): it was off while the platforms didn't move with the server
	GameMessages::Knockback knockback;
	knockback.target = target->GetObjectID();
	knockback.Caster = self->GetObjectID();
	knockback.Originator = self->GetObjectID();
	knockback.vector = newVec;
	knockback.Send(UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::PlayFXEffect(self->GetObjectID(), -1, u"knockback", "knockback").Send(UNASSIGNED_SYSTEM_ADDRESS);
}
