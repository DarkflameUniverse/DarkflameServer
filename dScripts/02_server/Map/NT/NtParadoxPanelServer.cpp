#include "NtParadoxPanelServer.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "CombatMessages.h"
#include "ObjectMessages.h"
#include "MissionComponent.h"
#include "EntityManager.h"
#include "Character.h"
#include "eMissionState.h"
#include "RenderComponent.h"
#include "eTerminateType.h"
#include "eStateChangeType.h"

void NtParadoxPanelServer::OnUse(Entity* self, Entity* user) {
	GameMessages::NotifyClientObject(self->GetObjectID(), u"bActive", 1, 0, user->GetObjectID(), "").Send(user->GetSystemAddress());

	GameMessages::TerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID()).Send(UNASSIGNED_SYSTEM_ADDRESS);

	self->SetVar(u"bActive", true);

	auto* missionComponent = user->GetComponent<MissionComponent>();

	const auto playerID = user->GetObjectID();

	for (const auto mission : tPlayerOnMissions) {
		if (missionComponent->GetMissionState(mission) != eMissionState::ACTIVE) {
			continue;
		}

		self->AddCallbackTimer(2, [this, self, playerID]() {
			auto* player = Game::entityManager->GetEntity(playerID);

			if (player == nullptr) {
				return;
			}

			const auto flag = self->GetVar<int32_t>(u"flag");

			auto* const character = player->GetCharacter();
			if (character) character->SetPlayerFlag(flag, true);

			RenderComponent::PlayAnimation(player, u"rebuild-celebrate");

			GameMessages::NotifyClientObject(self->GetObjectID(), u"SparkStop", 0, 0, player->GetObjectID(), "").Send(player->GetSystemAddress());
			GameMessages::SetStunned stun;
			stun.target = player->GetObjectID();
			stun.StateChangeType = eStateChangeType::POP;
			stun.bCantInteract = true;
			stun.bCantMove = true;
			stun.bCantTurn = true;
			stun.Send(player->GetSystemAddress());
			self->SetVar(u"bActive", false);
			});
		RenderComponent::PlayAnimation(user, u"nexus-powerpanel", 6.0f);
		GameMessages::SetStunned stun;
		stun.target = user->GetObjectID();
		stun.StateChangeType = eStateChangeType::PUSH;
		stun.bCantInteract = true;
		stun.bCantMove = true;
		stun.bCantTurn = true;
		stun.Send(user->GetSystemAddress());
		return;
	}

	RenderComponent::PlayAnimation(user, shockAnim);

	const auto dir = QuatUtils::Right(self->GetRotation());

	GameMessages::Knockback knockback;
	knockback.target = user->GetObjectID();
	knockback.Caster = self->GetObjectID();
	knockback.Originator = self->GetObjectID();
	knockback.vector = NiPoint3{ dir.x * 15, 5, dir.z * 15 };
	knockback.Send(UNASSIGNED_SYSTEM_ADDRESS);

	GameMessages::PlayFXEffect(self->GetObjectID(), 6432, u"create", "console_sparks").Send(UNASSIGNED_SYSTEM_ADDRESS);

	self->AddCallbackTimer(2, [this, self, playerID]() {
		auto* player = Game::entityManager->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		GameMessages::NotifyClientObject(self->GetObjectID(), u"bActive", 0, 0, player->GetObjectID(), "").Send(player->GetSystemAddress());

		GameMessages::StopFXEffect(self->GetObjectID(), true, "console_sparks").Send(UNASSIGNED_SYSTEM_ADDRESS);

		self->SetVar(u"bActive", false);
		});
}
