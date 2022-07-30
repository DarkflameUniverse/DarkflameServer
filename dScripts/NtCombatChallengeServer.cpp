#include "NtCombatChallengeServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "InventoryComponent.h"
#include "MissionComponent.h"

void NtCombatChallengeServer::OnUse(Entity* self, Entity* user) {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"UI_Open", 0, 0, user->GetObjectID(), "", user->GetSystemAddress());
}

void NtCombatChallengeServer::OnDie(Entity* self, Entity* killer) {
	if (killer != self && killer != nullptr) {
		SpawnTargetDummy(self);
	}
}


void NtCombatChallengeServer::OnHitOrHealResult(Entity* self, Entity* attacker, int32_t damage) {
	const auto playerID = self->GetVar<LWOOBJID>(u"playerID");

	auto* player = EntityManager::Instance()->GetEntity(playerID);

	if (player == nullptr) {
		return;
	}

	auto totalDmg = self->GetVar<int32_t>(u"totalDmg");

	totalDmg += damage;

	self->SetVar(u"totalDmg", totalDmg);
	self->SetNetworkVar(u"totalDmg", totalDmg);

	GameMessages::SendPlayNDAudioEmitter(self, attacker->GetSystemAddress(), scoreSound);
}


void NtCombatChallengeServer::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"UI_Close", 0, 0, sender->GetObjectID(), "", sender->GetSystemAddress());
}


void NtCombatChallengeServer::OnMessageBoxResponse(Entity* self, Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData) {
	if (identifier == u"PlayButton" && button == 1) {
		self->SetNetworkVar(u"bInUse", true);

		self->SetVar(u"playerID", sender->GetObjectID());

		auto* inventoryComponent = sender->GetComponent<InventoryComponent>();

		if (inventoryComponent != nullptr) {
			inventoryComponent->RemoveItem(3039, 1);
		}

		GameMessages::SendPlayNDAudioEmitter(self, sender->GetSystemAddress(), startSound);

		self->AddTimer("start_delay", 2.0f);

		GameMessages::SendShowActivityCountdown(self->GetObjectID(), false, false, u"", 0, sender->GetSystemAddress());

		self->SetNetworkVar(u"toggle", true);
	} else if (identifier == u"CloseButton") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"UI_Close", 1, 0, sender->GetObjectID(), "", sender->GetSystemAddress());
	}
}

void NtCombatChallengeServer::SpawnTargetDummy(Entity* self) {
	const auto playerID = self->GetVar<LWOOBJID>(u"playerID");

	auto* player = EntityManager::Instance()->GetEntity(playerID);

	if (player == nullptr) {
		return;
	}

	auto targetNumber = self->GetVar<int32_t>(u"TargetNumber");
	if (targetNumber == 0) targetNumber = 1;

	if (targetNumber > tTargets.size()) targetNumber = tTargets.size();

	self->SetVar<int32_t>(u"TargetNumber", targetNumber + 1);

	const auto dummyLOT = tTargets[targetNumber - 1];

	EntityInfo info{};
	info.lot = dummyLOT;
	info.spawnerID = self->GetObjectID();
	info.pos = self->GetPosition();
	info.rot = self->GetRotation();
	info.settings = { new LDFData<std::string>(u"custom_script_server", "scripts\\02_server\\Map\\NT\\L_NT_COMBAT_CHALLENGE_DUMMY.lua") };

	auto* dummy = EntityManager::Instance()->CreateEntity(info);

	dummy->SetVar(u"challengeObjectID", self->GetObjectID());

	EntityManager::Instance()->ConstructEntity(dummy);

	self->SetVar(u"currentDummy", dummy->GetObjectID());
}

void NtCombatChallengeServer::SetAttackImmunity(LWOOBJID objID, bool bTurnOn) {

}

void NtCombatChallengeServer::OnChildLoaded(Entity* self, Entity* child) {
	auto targetNumber = self->GetVar<int32_t>(u"TargetNumber");
	if (targetNumber == 0) targetNumber = 1;
	self->SetVar(u"TargetNumber", targetNumber + 1);

	const auto playerID = self->GetVar<LWOOBJID>(u"playerID");

	auto* player = EntityManager::Instance()->GetEntity(playerID);

	if (player == nullptr) {
		return;
	}

	child->SetRotation(NiQuaternion::LookAt(child->GetPosition(), player->GetPosition()));

	self->SetVar(u"currentTargetID", child->GetObjectID());

	EntityManager::Instance()->SerializeEntity(child);

	child->GetGroups().push_back("targets_" + std::to_string(self->GetObjectID()));
}

void NtCombatChallengeServer::ResetGame(Entity* self) {
	const auto totalDmg = self->GetVar<int32_t>(u"totalDmg");
	const auto playerID = self->GetVar<LWOOBJID>(u"playerID");

	auto* player = EntityManager::Instance()->GetEntity(playerID);

	if (player != nullptr) {
		auto* missionComponent = player->GetComponent<MissionComponent>();

		if (missionComponent != nullptr) {
			for (const auto& mission : tMissions) {
				if (totalDmg >= mission.damage) {
					missionComponent->ForceProgressTaskType(mission.mission, 1, 1);
				}
			}
		}
	}

	self->SetVar(u"TargetNumber", 1);
	self->SetVar(u"playerID", LWOOBJID_EMPTY);
	self->SetVar(u"totalDmg", 0);
	self->SetNetworkVar(u"totalDmg", false);
	self->SetNetworkVar(u"update_time", 0);

	const auto& targetObjs = EntityManager::Instance()->GetEntitiesInGroup("targets_" + std::to_string(self->GetObjectID()));

	for (auto* target : targetObjs) {
		target->Smash(self->GetObjectID());
	}

	const auto currentID = self->GetVar<LWOOBJID>(u"currentDummy");

	auto* current = EntityManager::Instance()->GetEntity(currentID);

	if (current != nullptr) {
		current->Smash(self->GetObjectID());
	}
}

void NtCombatChallengeServer::OnActivityTimerUpdate(Entity* self, float timeRemaining) {
	self->SetNetworkVar(u"update_time", std::ceil(timeRemaining));

	if (timeRemaining <= 3) {
		GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, timerLowSound);
	} else {
		GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, timerSound);
	}
}

void NtCombatChallengeServer::OnTimerDone(Entity* self, std::string timerName) {
	if (timerName == "start_delay") {
		self->SetVar(u"game_tick", gameTime);

		SpawnTargetDummy(self);

		self->AddTimer("game_tick", 1);

		self->SetNetworkVar(u"totalTime", gameTime);
	} else if (timerName == "game_tick") {
		auto gameTick = self->GetVar<float>(u"game_tick");

		gameTick -= 1;

		self->SetVar(u"game_tick", gameTick);

		if (gameTick <= 0) {
			ResetGame(self);

			GameMessages::SendPlayNDAudioEmitter(self, UNASSIGNED_SYSTEM_ADDRESS, stopSound);

			self->AddTimer("reset_tick", 5);
		} else {
			self->AddTimer("game_tick", 1);

			OnActivityTimerUpdate(self, gameTick);
		}
	} else if (timerName == "reset_tick") {
		self->SetNetworkVar(u"toggle", false);
		self->SetNetworkVar(u"bInUse", false);
	}
}
