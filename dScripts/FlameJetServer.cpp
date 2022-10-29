#include "FlameJetServer.h"
#include "SkillComponent.h"
#include "GameMessages.h"

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

	auto dir = target->GetRotation().GetForwardVector();

	dir.y = 25;
	dir.x = -dir.x * 15;
	dir.z = -dir.z * 15;

	GameMessages::SendKnockback(target->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 1000, dir);
}

void FlameJetServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
	Game::logger->Log("FlameJetServer::OnFireEventServerSide", "Event: %s", args.c_str());

	if (args == "OnActivated") {
		self->SetNetworkVar<bool>(u"FlameOn", false);
	} else if (args == "OnDectivated") {
		self->SetNetworkVar<bool>(u"FlameOn", true);
	}
}
