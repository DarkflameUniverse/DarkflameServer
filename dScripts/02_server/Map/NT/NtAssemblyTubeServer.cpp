#include "NtAssemblyTubeServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "eMissionTaskType.h"
#include "eMissionState.h"
#include "RenderComponent.h"
#include "eEndBehavior.h"
#include "eStateChangeType.h"

void NtAssemblyTubeServer::OnStartup(Entity* self) {
	self->SetProximityRadius(5, "teleport");
}

void NtAssemblyTubeServer::OnPlayerLoaded(Entity* self, Entity* player) {

}

void NtAssemblyTubeServer::OnProximityUpdate(Entity* self, Entity* entering, std::string name, std::string status) {
	if (status != "ENTER" || !entering->IsPlayer() || name != "teleport") return;

	auto* player = entering;
	const auto playerID = player->GetObjectID();

	RunAssemblyTube(self, player);

	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent != nullptr) {
		missionComponent->Progress(eMissionTaskType::SCRIPT, self->GetLOT());
	}
}

void NtAssemblyTubeServer::RunAssemblyTube(Entity* self, Entity* player) {
	const auto playerID = player->GetObjectID();

	const auto iter = m_TeleportingPlayerTable.find(playerID);
	if (iter == m_TeleportingPlayerTable.end()) m_TeleportingPlayerTable[playerID] = false;
	const auto bPlayerBeingTeleported = m_TeleportingPlayerTable[playerID];

	if (player->IsPlayer() && !bPlayerBeingTeleported) {
		auto teleCinematic = self->GetVar<std::u16string>(u"Cinematic");

		GameMessages::SendSetStunned(playerID, eStateChangeType::PUSH, player->GetSystemAddress(), LWOOBJID_EMPTY,
			true, true, true, true, true, true, true
		);

		if (!teleCinematic.empty()) {
			const auto teleCinematicUname = teleCinematic;
			GameMessages::SendPlayCinematic(player->GetObjectID(), teleCinematicUname, player->GetSystemAddress(),
				true, true, true, false, eEndBehavior::RETURN, false, -1, false, true
			);
		}

		RenderComponent::PlayAnimation(player, u"tube-sucker", 4.0f);

		const auto animTime = 3;

		self->AddCallbackTimer(animTime, [this, self, playerID]() {
			auto* player = Game::entityManager->GetEntity(playerID);

			if (player == nullptr) {
				return;
			}

			TeleportPlayer(self, player);
			});
	}
}

void NtAssemblyTubeServer::TeleportPlayer(Entity* self, Entity* player) {
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

	GameMessages::SendTeleport(player->GetObjectID(), destPosition, destRotation, player->GetSystemAddress(), true);

	RenderComponent::PlayAnimation(player, u"tube-resurrect", 4.0f);

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

void NtAssemblyTubeServer::UnlockPlayer(Entity* self, Entity* player) {
	const auto playerID = player->GetObjectID();

	m_TeleportingPlayerTable[playerID] = false;

	GameMessages::SendSetStunned(playerID, eStateChangeType::POP, player->GetSystemAddress(), LWOOBJID_EMPTY,
		true, true, true, true, true, true, true
	);
}
