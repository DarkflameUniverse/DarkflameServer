#include "CavePrisonCage.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "GameMessages.h"
#include "Character.h"
#include "dZoneManager.h"

void CavePrisonCage::OnStartup(Entity* self) {
	const auto& myNum = self->GetVar<std::u16string>(u"myNumber");

	if (myNum.empty()) {
		return;
	}

	auto* spawner = dZoneManager::Instance()->GetSpawnersByName("PrisonCounterweight_0" + GeneralUtils::UTF16ToWTF8(myNum))[0];

	self->SetVar<Spawner*>(u"CWSpawner", spawner);

	Setup(self, spawner);
}

void CavePrisonCage::Setup(Entity* self, Spawner* spawner) {
	SpawnCounterweight(self, spawner);

	NiPoint3 mypos = self->GetPosition();
	NiQuaternion myrot = self->GetRotation();

	mypos.y += 1.5;
	mypos.z -= 0.5;

	EntityInfo info{};
	info.lot = m_Villagers[self->GetVarAs<int32_t>(u"myNumber") - 1];
	info.pos = mypos;
	info.rot = myrot;
	info.spawnerID = self->GetObjectID();

	// Spawn the villager inside the jail
	auto* entity = EntityManager::Instance()->CreateEntity(info);

	// Save the villeger ID
	self->SetVar<LWOOBJID>(u"villager", entity->GetObjectID());

	// Construct the entity
	EntityManager::Instance()->ConstructEntity(entity);
}

void CavePrisonCage::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state != eRebuildState::REBUILD_RESETTING) {
		return;
	}

	auto* spawner = self->GetVar<Spawner*>(u"CWSpawner");

	if (spawner == nullptr) {
		return;
	}

	spawner->Reset();

	SpawnCounterweight(self, spawner);
}

void CavePrisonCage::SpawnCounterweight(Entity* self, Spawner* spawner) {
	spawner->Reset();

	auto* counterweight = spawner->Spawn();

	self->SetVar<LWOOBJID>(u"Counterweight", counterweight->GetObjectID());

	auto* rebuildComponent = counterweight->GetComponent<RebuildComponent>();

	if (rebuildComponent != nullptr) {
		rebuildComponent->AddRebuildStateCallback([this, self](eRebuildState state) {
			OnRebuildNotifyState(self, state);
			});

		rebuildComponent->AddRebuildCompleteCallback([this, self](Entity* user) {
			// The counterweight is a simple mover, which is not implemented, so we'll just set it's position
			auto* counterweight = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Counterweight"));

			if (counterweight == nullptr) {
				return;
			}

			// Move the counterweight down 2 units
			counterweight->SetPosition(counterweight->GetPosition() + NiPoint3(0, -2, 0));

			// Serialize the counterweight
			EntityManager::Instance()->SerializeEntity(counterweight);

			// notifyPlatformAtLastWaypoint

			// Save the userID as Builder
			self->SetVar<LWOOBJID>(u"Builder", user->GetObjectID());

			// Get the button and make sure it still exists
			auto* button = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Button"));

			if (button == nullptr) {
				return;
			}

			// Play the 'down' animation on the button
			GameMessages::SendPlayAnimation(button, u"down");

			// Setup a timer named 'buttonGoingDown' to be triggered in 5 seconds
			self->AddTimer("buttonGoingDown", 5.0f);
			});
	}

	if (self->GetVar<LWOOBJID>(u"Button")) {
		return;
	}

	GetButton(self);
}

void CavePrisonCage::GetButton(Entity* self) {
	const auto buttons = EntityManager::Instance()->GetEntitiesInGroup("PrisonButton_0" + std::to_string(self->GetVarAs<int32_t>(u"myNumber")));

	if (buttons.size() == 0) {
		// Try again in 0.5 seconds
		self->AddCallbackTimer(0.5, [this, self]() {
			GetButton(self);
			});

		return;
	}

	auto* button = buttons[0];

	self->SetVar<LWOOBJID>(u"Button", button->GetObjectID());
}

void CavePrisonCage::OnTimerDone(Entity* self, std::string timerName) {
	// the anim of the button down is over
	if (timerName == "buttonGoingDown") {
		// Play the 'up' animation
		GameMessages::SendPlayAnimation(self, u"up");

		// Setup a timer named 'CageOpen' to be triggered in 1 second
		self->AddTimer("CageOpen", 1.0f);
	} else if (timerName == "CageOpen") {
		// play the idle open anim
		GameMessages::SendPlayAnimation(self, u"idle-up");

		// Get the villeger
		auto* villager = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"villager"));

		if (villager == nullptr) {
			return;
		}

		GameMessages::SendNotifyClientObject(villager->GetObjectID(), u"TimeToChat", 0, 0, LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);

		// Get the builder and make sure it still exists
		auto* builder = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Builder"));

		if (builder == nullptr) {
			return;
		}

		const auto flagNum = 2020 + self->GetVarAs<int32_t>(u"myNumber");

		// Set the flag on the builder character
		builder->GetCharacter()->SetPlayerFlag(flagNum, true);

		// Setup a timer named 'VillagerEscape' to be triggered in 5 seconds
		self->AddTimer("VillagerEscape", 5.0f);
	} else if (timerName == "VillagerEscape") {
		// Get the villeger and make sure it still exists
		auto* villager = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"villager"));

		if (villager == nullptr) {
			return;
		}

		// Kill the villager
		villager->Kill();

		// Setup a timer named 'SmashCounterweight' to be triggered in 2 seconds
		self->AddTimer("SmashCounterweight", 2.0f);
	} else if (timerName == "SmashCounterweight") {
		// Get the counterweight and make sure it still exists
		auto* counterweight = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Counterweight"));

		if (counterweight == nullptr) {
			return;
		}

		// Smash the counterweight
		counterweight->Smash();

		// Get the button and make sure it still exists
		auto* button = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(u"Button"));

		if (button == nullptr) {
			return;
		}

		// Play the 'up' animation on the button
		GameMessages::SendPlayAnimation(button, u"up");

		// Setup a timer named 'CageClosed' to be triggered in 1 second
		self->AddTimer("CageClosed", 1.0f);
	} else if (timerName == "CageClosed") {
		// play the idle closed anim
		GameMessages::SendPlayAnimation(self, u"idle");

		// Setup a timer named 'ResetPrison' to be triggered in 10 seconds
		self->AddTimer("ResetPrison", 10.0f);
	} else if (timerName == "ResetPrison") {
		Setup(self, self->GetVar<Spawner*>(u"CWSpawner"));
	}
}
