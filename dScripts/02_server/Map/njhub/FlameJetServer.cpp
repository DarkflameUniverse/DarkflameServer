#include "FlameJetServer.h"
#include "SkillComponent.h"
#include "GameMessages.h"
#include "CombatMessages.h"

void FlameJetServer::OnStartup(Entity* self) {
	if (self->GetVar<bool>(u"NotActive")) {
		return;
	}

	self->SetNetworkVar<bool>(u"FlameOn", true);
}

void FlameJetServer::OnCollisionPhantom(Entity* self, Entity* target) {
	if (!target->IsPlayer()) {
		return;
	}

	if (!self->GetNetworkVar<bool>(u"FlameOn")) {
		return;
	}

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateBehavior(726, 11723, target->GetObjectID(), true);

	auto dir = QuatUtils::Forward(target->GetRotation());

	dir.y = 25;
	dir.x = -dir.x * 15;
	dir.z = -dir.z * 15;

	GameMessages::Knockback knockback;
	knockback.target = target->GetObjectID();
	knockback.Caster = self->GetObjectID();
	knockback.Originator = self->GetObjectID();
	knockback.iKnockBackTimeMS = 1000;
	knockback.vector = dir;
	knockback.Send(UNASSIGNED_SYSTEM_ADDRESS);
}

void FlameJetServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	LOG("Event: %s", args.c_str());

	if (args == "OnActivated") {
		self->SetNetworkVar<bool>(u"FlameOn", false);
	} else if (args == "OnDectivated") {
		self->SetNetworkVar<bool>(u"FlameOn", true);
	}
}
