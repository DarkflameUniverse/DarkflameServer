#include "AmSkullkinDrill.h"
#include "GameMessages.h"
#include "MovingPlatformComponent.h"
#include "DestroyableComponent.h"
#include "ProximityMonitorComponent.h"
#include "MissionComponent.h"

void AmSkullkinDrill::OnStartup(Entity* self) {
	self->SetNetworkVar(u"bIsInUse", false);
	self->SetVar(u"bActive", true);

	GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"spin", "active");

	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();

	if (movingPlatformComponent == nullptr) {
		return;
	}

	movingPlatformComponent->SetSerialized(true);

	movingPlatformComponent->GotoWaypoint(0);

	auto* standObj = GetStandObj(self);

	if (standObj != nullptr) {
		standObj->SetVar(u"bActive", true);
	}

	self->SetProximityRadius(5, "spin_distance");
}

Entity* AmSkullkinDrill::GetStandObj(Entity* self) {
	const auto& myGroup = self->GetGroups();

	if (myGroup.empty()) {
		return nullptr;
	}

	std::string groupName = "Drill_Stand_";

	groupName.push_back(myGroup[0][myGroup[0].size() - 1]);

	const auto standObjs = EntityManager::Instance()->GetEntitiesInGroup(groupName);

	if (standObjs.empty()) {
		return nullptr;
	}

	return standObjs[0];
}

void AmSkullkinDrill::OnSkillEventFired(Entity* self, Entity* caster, const std::string& message) {
	if (message != "NinjagoSpinEvent" || self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	}

	auto* proximityMonitorComponent = self->GetComponent<ProximityMonitorComponent>();

	if (proximityMonitorComponent == nullptr || !proximityMonitorComponent->IsInProximity("spin_distance", caster->GetObjectID())) {
		return;
	}

	self->SetVar(u"activaterID", caster->GetObjectID());

	self->SetNetworkVar(u"bIsInUse", true);

	TriggerDrill(self);
}

void AmSkullkinDrill::TriggerDrill(Entity* self) {
	GameMessages::SendPlayAnimation(self, u"slowdown");

	self->AddTimer("killDrill", 10.0f);

	auto* standObj = GetStandObj(self);

	if (standObj != nullptr) {
		standObj->SetVar(u"bActive", false);
	}

	auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();

	if (movingPlatformComponent == nullptr) {
		return;
	}

	movingPlatformComponent->GotoWaypoint(1);
}

void AmSkullkinDrill::OnWaypointReached(Entity* self, uint32_t waypointIndex) {
	if (waypointIndex == 1) {
		auto myPos = self->GetPosition();
		auto myRot = self->GetRotation();

		myPos.y -= 21;

		EntityInfo info = {};
		info.lot = 12346;
		info.pos = myPos;
		info.rot = myRot;
		info.scale = 3; // Needs the scale, otherwise attacks fail
		info.spawnerID = self->GetObjectID();

		auto* child = EntityManager::Instance()->CreateEntity(info);

		EntityManager::Instance()->ConstructEntity(child);

		self->SetVar(u"ChildSmash", child->GetObjectID());

		child->AddDieCallback([this, self]() {
			const auto& userID = self->GetVar<LWOOBJID>(u"activaterID");

			auto* player = EntityManager::Instance()->GetEntity(userID);

			if (player == nullptr) {
				return;
			}

			OnHitOrHealResult(self, player, 1);
			});
	}

	OnArrived(self, waypointIndex);
}

void AmSkullkinDrill::OnUse(Entity* self, Entity* user) {
	if (self->GetNetworkVar<bool>(u"bIsInUse")) {
		return;
	}

	self->SetNetworkVar(u"bIsInUse", true);

	GameMessages::SendPlayFXEffect(user->GetObjectID(), 5499, u"on-anim", "tornado");
	GameMessages::SendPlayFXEffect(user->GetObjectID(), 5502, u"on-anim", "staff");

	const auto userID = user->GetObjectID();

	self->SetVar(u"userID", userID);
	self->SetVar(u"activaterID", userID);

	PlayAnim(self, user, "spinjitzu-staff-windup");
	PlayCinematic(self);

	FreezePlayer(self, user, true);
}

void AmSkullkinDrill::FreezePlayer(Entity* self, Entity* player, bool bFreeze) {
	eStunState eChangeType = POP;

	if (bFreeze) {
		if (player->GetIsDead()) {
			return;
		}

		eChangeType = PUSH;
	} else {
		if (player->GetIsDead()) {
			//
		}
	}

	GameMessages::SendSetStunned(player->GetObjectID(), eChangeType, player->GetSystemAddress(), self->GetObjectID(),
		true, false, true, false, true, false, true
	);
}

void AmSkullkinDrill::OnArrived(Entity* self, uint32_t waypointIndex) {
	auto* standObj = GetStandObj(self);

	if (waypointIndex == 1) {
		GameMessages::SendPlayAnimation(self, u"no-spin");
		GameMessages::SendStopFXEffect(self, true, "active");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"indicator", "indicator");

		self->SetVar(u"bActive", false);

		const auto playerID = self->GetVar<LWOOBJID>(u"userID");

		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player != nullptr) {
			PlayAnim(self, player, "spinjitzu-staff-end");
		}

		if (standObj != nullptr) {
			standObj->SetVar(u"bActive", false);
		}

		return;
	} else {
		GameMessages::SendPlayAnimation(self, u"idle");
		GameMessages::SendPlayFXEffect(self->GetObjectID(), -1, u"spin", "active");
		GameMessages::SendStopFXEffect(self, true, "indicator");
	}
}

void AmSkullkinDrill::PlayCinematic(Entity* self) {
	auto* player = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"userID"));

	if (player == nullptr) {
		return;
	}

	const auto& cine = self->GetVar<std::u16string>(u"cinematic");

	if (cine.empty()) {
		return;
	}

	GameMessages::SendPlayCinematic(player->GetObjectID(), cine, player->GetSystemAddress());
}

void AmSkullkinDrill::PlayAnim(Entity* self, Entity* player, const std::string& animName) {
	const auto animTime = animName == "spinjitzu-staff-end" ? 0.5f : 1.0f;

	GameMessages::SendPlayAnimation(player, GeneralUtils::ASCIIToUTF16(animName));

	self->AddTimer("AnimDone_" + animName, animTime);
}

void AmSkullkinDrill::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();

	if (destroyableComponent == nullptr || !attacker->IsPlayer()) {
		return;
	}

	if (self->GetVar<bool>(u"bActive")) {
		return;
	}

	const auto activaterID = self->GetVar<LWOOBJID>(u"activaterID");

	auto* activator = EntityManager::Instance()->GetEntity(activaterID);

	// TODO: Missions
	if (activator != nullptr) {
		auto* missionComponent = activator->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			for (const auto missionID : m_MissionsToUpdate) {
				missionComponent->ForceProgressValue(missionID, 1, self->GetLOT());
			}
		}
	}

	self->Smash(attacker->GetObjectID(), SILENT);

	self->CancelAllTimers();

	auto* standObj = GetStandObj(self);

	if (standObj != nullptr) {
		GameMessages::SendPlayFXEffect(standObj->GetObjectID(), 4946, u"explode", "explode");
	}
}

void AmSkullkinDrill::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "killDrill") {
		const auto childID = self->GetVar<LWOOBJID>(u"ChildSmash");

		auto* child = EntityManager::Instance()->GetEntity(childID);

		if (child != nullptr) {
			child->Smash(self->GetObjectID(), SILENT);
		}

		self->SetNetworkVar(u"bIsInUse", false);
		self->SetVar(u"bActive", true);
		self->SetVar(u"activaterID", LWOOBJID_EMPTY);

		auto* standObj = GetStandObj(self);

		if (standObj != nullptr) {
			standObj->SetVar(u"bActive", true);
		}

		auto* movingPlatformComponent = self->GetComponent<MovingPlatformComponent>();

		if (movingPlatformComponent == nullptr) {
			return;
		}

		movingPlatformComponent->GotoWaypoint(0);

		return;
	}

	const auto& data = GeneralUtils::SplitString(timerName, '_');

	if (data.empty()) {
		return;
	}

	if (data[0] == "AnimDone") {
		const auto& animName = data[1];

		const auto playerID = self->GetVar<LWOOBJID>(u"userID");

		auto* player = EntityManager::Instance()->GetEntity(playerID);

		if (player == nullptr) {
			return;
		}

		if (animName == "spinjitzu-staff-windup") {
			TriggerDrill(self);

			GameMessages::SendPlayAnimation(player, u"spinjitzu-staff-loop");
		} else if (animName == "spinjitzu-staff-end") {
			FreezePlayer(self, player, false);

			self->SetVar(u"userID", LWOOBJID_EMPTY);

			GameMessages::SendStopFXEffect(player, true, "tornado");
			GameMessages::SendStopFXEffect(player, true, "staff");
		}

	} else if (data[0] == "TryUnFreezeAgain") {

	}
}
