#include "GfCaptainsCannon.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "MissionComponent.h"

void GfCaptainsCannon::OnUse(Entity* self, Entity* user) {
	if (self->GetVar<bool>(u"bIsInUse")) {
		return;
	}

	self->SetVar<LWOOBJID>(u"userID", user->GetObjectID());

	self->SetVar<bool>(u"bIsInUse", true);
	self->SetNetworkVar<bool>(u"bIsInUse", true);

	GameMessages::SendSetStunned(user->GetObjectID(), PUSH, user->GetSystemAddress(),
		LWOOBJID_EMPTY, true, true, true, true, true, true, true, true
	);

	auto position = self->GetPosition();
	auto forward = self->GetRotation().GetForwardVector();

	position.x += forward.x * -3;
	position.z += forward.z * -3;

	auto rotation = self->GetRotation();

	GameMessages::SendTeleport(user->GetObjectID(), position, rotation, user->GetSystemAddress());

	GameMessages::SendPlayAnimation(user, u"cannon-strike-no-equip");

	GameMessages::SendPlayFXEffect(user->GetObjectID(), 6039, u"hook", "hook", LWOOBJID_EMPTY, 1, 1, true);

	self->AddTimer("FireCannon", 1.667f);
}

void GfCaptainsCannon::OnTimerDone(Entity* self, std::string timerName) {
	const auto playerId = self->GetVar<LWOOBJID>(u"userID");

	auto* player = EntityManager::Instance()->GetEntity(playerId);

	if (player == nullptr) {
		self->SetVar<bool>(u"bIsInUse", false);
		self->SetNetworkVar<bool>(u"bIsInUse", false);

		return;
	}

	if (timerName == "FireCannon") {
		float cinematicTime = 6.3f;

		GameMessages::SendPlayCinematic(playerId, u"Cannon_Cam", player->GetSystemAddress());

		self->AddTimer("cinematicTimer", cinematicTime);

		const auto sharkObjects = EntityManager::Instance()->GetEntitiesInGroup("SharkCannon");

		for (auto* shark : sharkObjects) {
			if (shark->GetLOT() != m_SharkItemID) continue;

			GameMessages::SendPlayAnimation(shark, u"cannon");
		}

		GameMessages::SendPlay2DAmbientSound(player, "{7457d85c-4537-4317-ac9d-2f549219ea87}");
	} else if (timerName == "cinematicTimer") {
		GameMessages::SendSetStunned(playerId, POP, player->GetSystemAddress(),
			LWOOBJID_EMPTY, true, true, true, true, true, true, true, true
		);

		self->SetVar<bool>(u"bIsInUse", false);
		self->SetNetworkVar<bool>(u"bIsInUse", false);

		GameMessages::SendStopFXEffect(player, true, "hook");

		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			missionComponent->ForceProgress(601, 910, 1);
		}

		GameMessages::SendTerminateInteraction(playerId, FROM_INTERACTION, self->GetObjectID());
	}
}
