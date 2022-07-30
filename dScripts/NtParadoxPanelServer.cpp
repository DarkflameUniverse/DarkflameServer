#include "NtParadoxPanelServer.h"
#include "GameMessages.h"
#include "MissionComponent.h"
#include "EntityManager.h"
#include "Character.h"

void NtParadoxPanelServer::OnUse(Entity* self, Entity* user) {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"bActive", 1, 0, user->GetObjectID(), "", user->GetSystemAddress());

	GameMessages::SendTerminateInteraction(user->GetObjectID(), eTerminateType::FROM_INTERACTION, self->GetObjectID());

	self->SetVar(u"bActive", true);

	auto* missionComponent = user->GetComponent<MissionComponent>();

	const auto playerID = user->GetObjectID();

	for (const auto mission : tPlayerOnMissions) {
		if (missionComponent->GetMissionState(mission) != MissionState::MISSION_STATE_ACTIVE) {
			continue;
		}

		self->AddCallbackTimer(2, [this, self, playerID]() {
			auto* player = EntityManager::Instance()->GetEntity(playerID);

			if (player == nullptr) {
				return;
			}

			const auto flag = self->GetVar<int32_t>(u"flag");

			player->GetCharacter()->SetPlayerFlag(flag, true);

			GameMessages::SendPlayAnimation(player, u"rebuild-celebrate");

			GameMessages::SendNotifyClientObject(self->GetObjectID(), u"SparkStop", 0, 0, player->GetObjectID(), "", player->GetSystemAddress());
			GameMessages::SendSetStunned(player->GetObjectID(), eStunState::POP, player->GetSystemAddress(), LWOOBJID_EMPTY, false, false, true, false, true, true, false, false, true);
			self->SetVar(u"bActive", false);
			});
		GameMessages::SendPlayAnimation(user, u"nexus-powerpanel", 6.0f);
		GameMessages::SendSetStunned(user->GetObjectID(), eStunState::PUSH, user->GetSystemAddress(), LWOOBJID_EMPTY, false, false, true, false, true, true, false, false, true);
		return;
	}

	GameMessages::SendPlayAnimation(user, shockAnim);

	const auto dir = self->GetRotation().GetRightVector();

	GameMessages::SendKnockback(user->GetObjectID(), self->GetObjectID(), self->GetObjectID(), 0, { dir.x * 15, 5, dir.z * 15 });

	GameMessages::SendPlayFXEffect(self, 6432, u"create", "console_sparks", LWOOBJID_EMPTY, 1.0, 1.0, true);

	self->AddCallbackTimer(2, [this, self, playerID]() {
		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"bActive", 0, 0, player->GetObjectID(), "", player->GetSystemAddress());

		GameMessages::SendStopFXEffect(self, true, "console_sparks");

		self->SetVar(u"bActive", false);
		});
}
