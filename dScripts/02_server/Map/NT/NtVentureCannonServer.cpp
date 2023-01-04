#include "NtVentureCannonServer.h"
#include "GameMessages.h"
#include "EntityManager.h"

void NtVentureCannonServer::OnUse(Entity* self, Entity* user) {
	auto* player = user;
	const auto playerID = player->GetObjectID();

	auto enterCinematic = self->GetVar<std::u16string>(u"EnterCinematic");

	if (enterCinematic.empty()) {
		return;
	}

	self->SetNetworkVar(u"bIsInUse", true);

	GameMessages::SendSetStunned(playerID, PUSH, player->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	auto destPosition = self->GetPosition();

	destPosition.y += 5 - 1.57f;

	auto destRotation = self->GetRotation();

	GameMessages::SendTeleport(playerID, destPosition, destRotation, player->GetSystemAddress(), true);

	GameMessages::SendPlayAnimation(player, u"scale-down", 4.0f);

	const auto enterCinematicUname = enterCinematic;
	GameMessages::SendPlayCinematic(player->GetObjectID(), enterCinematicUname, player->GetSystemAddress());

	GameMessages::SendPlayNDAudioEmitter(player, player->GetSystemAddress(), "{e8bf79ce-7453-4a7d-b872-fee65e97ff15}");

	self->AddCallbackTimer(3, [this, self]() {
		self->SetNetworkVar(u"bIsInUse", false);
		});

	self->AddCallbackTimer(1.5f, [this, self, playerID]() {
		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		EnterCannonEnded(self, player);
		});
}

void NtVentureCannonServer::EnterCannonEnded(Entity* self, Entity* player) {
	const auto playerID = player->GetObjectID();

	const auto& cannonEffectGroup = EntityManager::Instance()->GetEntitiesInGroup("cannonEffect");

	if (!cannonEffectGroup.empty()) {
		auto* cannonEffect = cannonEffectGroup[0];

		GameMessages::SendPlayFXEffect(cannonEffect, 6036, u"create", "cannon_blast", LWOOBJID_EMPTY, 1, 1, true);

		GameMessages::SendPlayEmbeddedEffectOnAllClientsNearObject(cannonEffect, u"camshake-bridge", cannonEffect->GetObjectID(), 100);
	}

	FirePlayer(self, player);

	auto exitCinematic = self->GetVar<std::u16string>(u"ExitCinematic");

	if (exitCinematic.empty()) {
		UnlockCannonPlayer(self, player);

		return;
	}

	const auto exitCinematicUname = exitCinematic;
	GameMessages::SendPlayCinematic(player->GetObjectID(), exitCinematicUname, player->GetSystemAddress(),
		true, true, true, false, 0, false, 0, false, false
	);

	self->AddCallbackTimer(1.5f, [this, self, playerID]() {
		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		ExitCannonEnded(self, player);
		});
}

void NtVentureCannonServer::ExitCannonEnded(Entity* self, Entity* player) {
	UnlockCannonPlayer(self, player);
}

void NtVentureCannonServer::UnlockCannonPlayer(Entity* self, Entity* player) {
	GameMessages::SendSetStunned(player->GetObjectID(), POP, player->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	self->SetNetworkVar(u"bIsInUse", false);

	GameMessages::SendTerminateInteraction(player->GetObjectID(), FROM_INTERACTION, self->GetObjectID());
}

void NtVentureCannonServer::FirePlayer(Entity* self, Entity* player) {
	auto destinationGroup = self->GetVar<std::u16string>(u"teleGroup");
	auto* destination = self;

	if (!destinationGroup.empty()) {
		const auto& groupObjs = EntityManager::Instance()->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(destinationGroup));

		if (!groupObjs.empty()) {
			destination = groupObjs[0];
		}
	}

	const auto destPosition = destination->GetPosition();
	const auto destRotation = destination->GetRotation();

	GameMessages::SendTeleport(player->GetObjectID(), destPosition, destRotation, player->GetSystemAddress(), true);

	GameMessages::SendPlayAnimation(player, u"venture-cannon-out", 4.0f);
}
