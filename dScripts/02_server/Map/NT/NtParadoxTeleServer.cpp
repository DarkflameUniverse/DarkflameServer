#include "NtParadoxTeleServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "RenderComponent.h"
#include "eStateChangeType.h"

void NtParadoxTeleServer::OnStartup(Entity* self) {
	self->SetProximityRadius(5, "teleport");
}

void NtParadoxTeleServer::OnPlayerLoaded(Entity* self, Entity* player) {

}

void NtParadoxTeleServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (status != "ENTER" || !entering->IsPlayer() || name != "teleport") return;

	auto* player = entering;
	const auto playerID = player->GetObjectID();

	const auto iter = m_TeleportingPlayerTable.find(playerID);
	if (iter == m_TeleportingPlayerTable.end()) m_TeleportingPlayerTable[playerID] = false;
	const auto bPlayerBeingTeleported = m_TeleportingPlayerTable[playerID];

	if (player->IsPlayer() && !bPlayerBeingTeleported) {
		GameMessages::SendSetStunned(playerID, eStateChangeType::PUSH, player->GetSystemAddress(), LWOOBJID_EMPTY,
			true, true, true, true, true, true, true
		);

		auto animTime = RenderComponent::PlayAnimation(player, u"teledeath", 4.0f);
		if (animTime == 0.0f) animTime = 2.0f;

		self->AddCallbackTimer(animTime, [this, self, playerID]() {
			auto* player = Game::entityManager->GetEntity(playerID);

			if (player == nullptr) {
				return;
			}

			TeleportPlayer(self, player);
			});
	}

	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->Progress(eMissionTaskType::SCRIPT, self->GetLOT());
	}
}

void NtParadoxTeleServer::TeleportPlayer(Entity* self, Entity* player) {
	auto destinationGroup = self->GetVar<std::u16string>(u"teleGroup");
	auto* destination = self;

	if (!destinationGroup.empty()) {
		const auto& groupObjs = Game::entityManager->GetEntitiesInGroup(GeneralUtils::UTF16ToWTF8(destinationGroup));

		if (!groupObjs.empty()) {
			destination = groupObjs[0];
		}
	}

	const auto destPosition = destination->GetPosition();
	const auto destRotation = destination->GetRotation();

	auto teleCinematic = self->GetVar<std::u16string>(u"Cinematic");

	if (!teleCinematic.empty()) {
		const auto teleCinematicUname = teleCinematic;
		GameMessages::SendPlayCinematic(player->GetObjectID(), teleCinematicUname, player->GetSystemAddress());
	}

	GameMessages::SendTeleport(player->GetObjectID(), destPosition, destRotation, player->GetSystemAddress(), true);

	RenderComponent::PlayAnimation(player, u"paradox-teleport-in", 4.0f);

	const auto animTime = 2;

	const auto playerID = player->GetObjectID();

	self->AddCallbackTimer(animTime, [this, self, playerID]() {
		auto* player = Game::entityManager->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		UnlockPlayer(self, player);
		});

	const auto useSound = self->GetVar<std::string>(u"sound1");

	if (!useSound.empty()) {
		GameMessages::SendPlayNDAudioEmitter(player, player->GetSystemAddress(), useSound);
	}
}

void NtParadoxTeleServer::UnlockPlayer(Entity* self, Entity* player) {
	const auto playerID = player->GetObjectID();

	m_TeleportingPlayerTable[playerID] = false;

	GameMessages::SendSetStunned(playerID, eStateChangeType::POP, player->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);

	auto teleCinematic = self->GetVar<std::u16string>(u"Cinematic");

	if (!teleCinematic.empty()) {
		const auto teleCinematicUname = teleCinematic;
		GameMessages::SendEndCinematic(player->GetObjectID(), teleCinematicUname, player->GetSystemAddress());
	}
}
