#include "AmDrawBridge.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "SimplePhysicsComponent.h"

void AmDrawBridge::OnStartup(Entity* self) {
	self->SetNetworkVar(u"InUse", false);
	self->SetVar(u"BridgeDown", false);
}

void AmDrawBridge::OnUse(Entity* self, Entity* user) {
	auto* bridge = GetBridge(self);

	if (bridge == nullptr) {
		return;
	}

	if (!self->GetNetworkVar<bool>(u"InUse")) {
		self->SetNetworkVar(u"startEffect", 5);

		self->AddTimer("ChangeBridge", 5);

		self->SetNetworkVar(u"InUse", true);
	}

	auto* player = user;

	GameMessages::SendTerminateInteraction(player->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
}

void AmDrawBridge::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "ChangeBridge") {
		auto* bridge = GetBridge(self);

		if (bridge == nullptr) {
			return;
		}

		if (!self->GetVar<bool>(u"BridgeDown")) {
			self->SetVar(u"BridgeDown", true);

			MoveBridgeDown(self, bridge, true);
		} else {
			self->SetVar(u"BridgeDown", false);

			MoveBridgeDown(self, bridge, false);
		}

		self->SetNetworkVar(u"BridgeLeaving", true);
		self->SetVar(u"BridgeDown", false);
	} else if (timerName == "SmashEffectBridge") {
		self->SetNetworkVar(u"SmashBridge", 5);
	} else if (timerName == "rotateBridgeDown") {
		auto* bridge = GetBridge(self);

		if (bridge == nullptr) {
			return;
		}

		self->SetNetworkVar(u"BridgeLeaving", false);

		auto* simplePhysicsComponent = bridge->GetComponent<SimplePhysicsComponent>();

		if (simplePhysicsComponent == nullptr) {
			return;
		}

		simplePhysicsComponent->SetAngularVelocity(NiPoint3::ZERO);

		EntityManager::Instance()->SerializeEntity(bridge);
	}
}

void AmDrawBridge::OnNotifyObject(Entity* self, Entity* sender, const std::string& name, int32_t param1, int32_t param2) {
	if (name == "BridgeBuilt") {
		self->SetVar(u"BridgeID", sender->GetObjectID());

		self->AddTimer("SmashEffectBridge", 45);

		self->SetNetworkVar(u"BridgeDead", true);

		sender->AddDieCallback([this, self, sender]() {
			NotifyDie(self, sender);
			});
	}
}

void AmDrawBridge::MoveBridgeDown(Entity* self, Entity* bridge, bool down) {
	auto* simplePhysicsComponent = bridge->GetComponent<SimplePhysicsComponent>();

	if (simplePhysicsComponent == nullptr) {
		return;
	}

	auto forwardVect = simplePhysicsComponent->GetRotation().GetForwardVector();

	auto degrees = down ? 90.0f : -90.0f;

	const auto travelTime = 2.0f;

	forwardVect = forwardVect * (float)((degrees / travelTime) * (3.14f / 180.0f));

	simplePhysicsComponent->SetAngularVelocity(forwardVect);

	EntityManager::Instance()->SerializeEntity(bridge);

	self->AddTimer("rotateBridgeDown", travelTime);
}

void AmDrawBridge::NotifyDie(Entity* self, Entity* other) {
	self->SetNetworkVar(u"InUse", false);
	self->SetVar(u"BridgeDown", false);

	self->CancelAllTimers();
}

Entity* AmDrawBridge::GetBridge(Entity* self) {
	const auto bridgeID = self->GetVar<LWOOBJID>(u"BridgeID");

	return EntityManager::Instance()->GetEntity(bridgeID);
}
