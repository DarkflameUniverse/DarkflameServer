#include "AmSkullkinDrillStand.h"
#include "GameMessages.h"
#include "dpShapeBox.h"
#include "Entity.h"
#include "RenderComponent.h"

void AmSkullkinDrillStand::OnStartup(Entity* self) {
	self->SetVar(u"bActive", true);

	self->SetProximityRadius(BoxDimensions(6.0f, 14.0f, 6.0f), "knockback");
}

void AmSkullkinDrillStand::OnNotifyObject(Entity* self, Entity* sender, const std::u16string& name, int32_t param1, int32_t param2) {

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

	RenderComponent::PlayAnimation(entering, u"knockback-recovery");
}
