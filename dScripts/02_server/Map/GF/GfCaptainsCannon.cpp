#include "GfCaptainsCannon.h"
#include "GameMessages.h"
#include "EffectsMessages.h"
#include "CombatMessages.h"
#include "EntityManager.h"
#include "MissionComponent.h"
#include "RenderComponent.h"
#include "eTerminateType.h"
#include "eStateChangeType.h"

void GfCaptainsCannon::OnUse(Entity* self, Entity* user) {
	if (self->GetVar<bool>(u"bIsInUse")) {
		return;
	}

	self->SetVar<LWOOBJID>(u"userID", user->GetObjectID());

	self->SetVar<bool>(u"bIsInUse", true);
	self->SetNetworkVar<bool>(u"bIsInUse", true);

	GameMessages::SetStunned stun;
	stun.target = user->GetObjectID();
	stun.StateChangeType = eStateChangeType::PUSH;
	stun.bCantAttack = true;
	stun.bCantEquip = true;
	stun.bCantInteract = true;
	stun.bCantJump = true;
	stun.bCantMove = true;
	stun.bCantTurn = true;
	stun.bCantUseItem = true;
	stun.bDontTerminateInteract = true;
	stun.Send(user->GetSystemAddress());

	auto position = self->GetPosition();
	auto forward = QuatUtils::Forward(self->GetRotation());

	position.x += forward.x * -3;
	position.z += forward.z * -3;

	auto rotation = self->GetRotation();

	GameMessages::SendTeleport(user->GetObjectID(), position, rotation, user->GetSystemAddress());

	RenderComponent::PlayAnimation(user, u"cannon-strike-no-equip");

	GameMessages::PlayFXEffect(user->GetObjectID(), 6039, u"hook", "hook").Send(UNASSIGNED_SYSTEM_ADDRESS);

	self->AddTimer("FireCannon", 1.667f);
}

void GfCaptainsCannon::OnTimerDone(Entity* self, std::string timerName) {
	const auto playerId = self->GetVar<LWOOBJID>(u"userID");

	auto* player = Game::entityManager->GetEntity(playerId);

	if (player == nullptr) {
		self->SetVar<bool>(u"bIsInUse", false);
		self->SetNetworkVar<bool>(u"bIsInUse", false);

		return;
	}

	if (timerName == "FireCannon") {
		float cinematicTime = 6.3f;

		GameMessages::PlayCinematic cinematic;
		cinematic.target = playerId;
		cinematic.pathName = u"Cannon_Cam";
		cinematic.Send(player->GetSystemAddress());

		self->AddTimer("cinematicTimer", cinematicTime);

		const auto sharkObjects = Game::entityManager->GetEntitiesInGroup("SharkCannon");

		for (auto* shark : sharkObjects) {
			if (shark->GetLOT() != m_SharkItemID) continue;

			RenderComponent::PlayAnimation(shark, u"cannon");
		}

		GameMessages::Play2DAmbientSound ambientSound;
		ambientSound.target = player->GetObjectID();
		ambientSound.audioGUID = "{7457d85c-4537-4317-ac9d-2f549219ea87}";
		ambientSound.SendToClient(player->GetSystemAddress());
	} else if (timerName == "cinematicTimer") {
		GameMessages::SetStunned stun;
		stun.target = playerId;
		stun.StateChangeType = eStateChangeType::POP;
		stun.bCantAttack = true;
		stun.bCantEquip = true;
		stun.bCantInteract = true;
		stun.bCantJump = true;
		stun.bCantMove = true;
		stun.bCantTurn = true;
		stun.bCantUseItem = true;
		stun.bDontTerminateInteract = true;
		stun.Send(player->GetSystemAddress());

		self->SetVar<bool>(u"bIsInUse", false);
		self->SetNetworkVar<bool>(u"bIsInUse", false);

		GameMessages::StopFXEffect(player->GetObjectID(), true, "hook").Send(UNASSIGNED_SYSTEM_ADDRESS);

		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			missionComponent->ForceProgress(601, 910, 1);
		}

		GameMessages::SendTerminateInteraction(playerId, eTerminateType::FROM_INTERACTION, self->GetObjectID());
	}
}
