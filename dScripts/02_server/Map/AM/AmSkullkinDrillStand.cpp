#include "AmSkullkinDrillStand.h"
#include "GameMessages.h"
#include "dpEntity.h"

void AmSkullkinDrillStand::OnStartup(Entity* self) {
	self->SetVar(u"bActive", true);

	self->SetProximityRadius(new dpEntity(self->GetObjectID(), { 6, 14, 6 }), "knockback");
}

void AmSkullkinDrillStand::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {

}

void AmSkullkinDrillStand::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (!self->GetVar<bool>(u"bActive")) {
		return;
	}

	if (!entering->IsPlayer() || status != "ENTER" || name != "knockback") {
		return;
	}

	auto myPos = self->GetPosition();

	auto objPos = entering->GetPosition();

	NiPoint3 newVec = { (objPos.x - myPos.x) * 4.5f, 15, (objPos.z - myPos.z) * 4.5f };

	GameMessages::SendKnockback(entering->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 0, newVec);

	GameMessages::SendPlayFXEffect(entering->GetObjectID(), 1378, u"create", "pushBack");

	GameMessages::SendPlayAnimation(entering, u"knockback-recovery");
}
