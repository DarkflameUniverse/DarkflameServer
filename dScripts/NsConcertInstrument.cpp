#include "NsConcertInstrument.h"
#include "GameMessages.h"
#include "Item.h"
#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "RebuildComponent.h"
#include "SoundTriggerComponent.h"
#include "MissionComponent.h"

// Constants are at the bottom

void NsConcertInstrument::OnStartup(Entity* self) {
	self->SetVar<bool>(u"beingPlayed", false);
	self->SetVar<LWOOBJID>(u"activePlayer", LWOOBJID_EMPTY);
	self->SetVar<LWOOBJID>(u"oldItemLeft", LWOOBJID_EMPTY);
	self->SetVar<LWOOBJID>(u"oldItemRight", LWOOBJID_EMPTY);
}

void NsConcertInstrument::OnRebuildNotifyState(Entity* self, eRebuildState state) {
	if (state == REBUILD_RESETTING || state == REBUILD_OPEN) {
		self->SetVar<LWOOBJID>(u"activePlayer", LWOOBJID_EMPTY);
	}
}

void NsConcertInstrument::OnRebuildComplete(Entity* self, Entity* target) {
	if (!target->GetIsDead()) {
		self->SetVar<LWOOBJID>(u"activePlayer", target->GetObjectID());

		self->AddCallbackTimer(0.2f, [self, target]() {
			RepositionPlayer(self, target);
			if (hideInstrumentOnPlay.at(GetInstrumentLot(self)))
				self->SetNetworkVar<bool>(u"Hide", true);
			});

		self->AddCallbackTimer(0.1f, [self, target]() {
			StartPlayingInstrument(self, target);
			});
	}
}

void NsConcertInstrument::OnFireEventServerSide(Entity* self, Entity* sender, std::string args, int32_t param1,
	int32_t param2, int32_t param3) {
	if (args == "stopPlaying") {
		const auto activePlayerID = self->GetVar<LWOOBJID>(u"activePlayer");
		if (activePlayerID == LWOOBJID_EMPTY)
			return;

		const auto activePlayer = EntityManager::Instance()->GetEntity(activePlayerID);
		if (activePlayer == nullptr)
			return;

		StopPlayingInstrument(self, activePlayer);
	}
}

void NsConcertInstrument::OnTimerDone(Entity* self, std::string name) {
	const auto activePlayerID = self->GetVar<LWOOBJID>(u"activePlayer");
	if (activePlayerID == LWOOBJID_EMPTY)
		return;

	// If for some reason the player becomes null (for example an unexpected leave), we need to clean up
	const auto activePlayer = EntityManager::Instance()->GetEntity(activePlayerID);
	if (activePlayer == nullptr && name != "cleanupAfterStop") {
		StopPlayingInstrument(self, nullptr);
		return;
	}

	if (activePlayer != nullptr && name == "checkPlayer" && self->GetVar<bool>(u"beingPlayed")) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"checkMovement", 0, 0,
			activePlayer->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
		auto* stats = activePlayer->GetComponent<DestroyableComponent>();
		if (stats) {
			if (stats->GetImagination() > 0) {
				self->AddTimer("checkPlayer", updateFrequency);
			} else {
				StopPlayingInstrument(self, activePlayer);
			}
		}
	} else if (activePlayer != nullptr && name == "deductImagination" && self->GetVar<bool>(u"beingPlayed")) {
		auto* stats = activePlayer->GetComponent<DestroyableComponent>();
		if (stats)
			stats->SetImagination(stats->GetImagination() - instrumentImaginationCost);

		self->AddTimer("deductImagination", instrumentCostFrequency);
	} else if (name == "cleanupAfterStop") {
		if (activePlayer != nullptr) {
			UnEquipInstruments(self, activePlayer);
			GameMessages::SendNotifyClientObject(self->GetObjectID(), u"stopPlaying", 0, 0,
				activePlayer->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
		}

		auto* rebuildComponent = self->GetComponent<RebuildComponent>();
		if (rebuildComponent != nullptr)
			rebuildComponent->ResetRebuild(false);

		self->Smash(self->GetObjectID(), VIOLENT);
		self->SetVar<LWOOBJID>(u"activePlayer", LWOOBJID_EMPTY);
	} else if (activePlayer != nullptr && name == "achievement") {
		auto* missionComponent = activePlayer->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->ForceProgress(302, 462, self->GetLOT());
		}
		self->AddTimer("achievement2", 10.0f);
	} else if (activePlayer != nullptr && name == "achievement2") {
		auto* missionComponent = activePlayer->GetComponent<MissionComponent>();
		if (missionComponent != nullptr) {
			missionComponent->ForceProgress(602, achievementTaskID.at(GetInstrumentLot(self)), self->GetLOT());
		}
	}
}

void NsConcertInstrument::StartPlayingInstrument(Entity* self, Entity* player) {
	const auto instrumentLot = GetInstrumentLot(self);
	self->SetVar<bool>(u"beingPlayed", true);

	// Stuff to notify the player
	EquipInstruments(self, player);
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"startPlaying", 0, 0,
		player->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
	GameMessages::SendPlayCinematic(player->GetObjectID(), cinematics.at(instrumentLot), UNASSIGNED_SYSTEM_ADDRESS);
	self->AddCallbackTimer(1.0f, [player, instrumentLot]() {
		GameMessages::SendPlayAnimation(player, animations.at(instrumentLot), 2.0f);
		});

	for (auto* soundBox : EntityManager::Instance()->GetEntitiesInGroup("Audio-Concert")) {
		auto* soundTrigger = soundBox->GetComponent<SoundTriggerComponent>();
		if (soundTrigger != nullptr) {
			soundTrigger->ActivateMusicCue(music.at(instrumentLot));
		}
	}

	// Add timers for deducting imagination and checking if the instruments can still be played
	self->AddTimer("checkPlayer", updateFrequency);
	self->AddTimer("deductImagination", instrumentCostFrequency);
	self->AddTimer("achievement", 20.0f);
}

void NsConcertInstrument::StopPlayingInstrument(Entity* self, Entity* player) {
	// No use in stopping twice
	if (!self->GetVar<bool>(u"beingPlayed"))
		return;

	const auto instrumentLot = GetInstrumentLot(self);

	// Player might be null if they left
	if (player != nullptr) {
		auto* missions = player->GetComponent<MissionComponent>();
		if (missions != nullptr && missions->GetMissionState(176) == MissionState::MISSION_STATE_ACTIVE) {
			missions->Progress(MissionTaskType::MISSION_TASK_TYPE_SCRIPT, self->GetLOT());
		}

		GameMessages::SendEndCinematic(player->GetObjectID(), cinematics.at(instrumentLot), UNASSIGNED_SYSTEM_ADDRESS, 1.0f);
		GameMessages::SendPlayAnimation(player, smashAnimations.at(instrumentLot), 2.0f);
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"stopCheckingMovement", 0, 0,
			player->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
	}

	self->SetVar<bool>(u"beingPlayed", false);

	for (auto* soundBox : EntityManager::Instance()->GetEntitiesInGroup("Audio-Concert")) {
		auto* soundTrigger = soundBox->GetComponent<SoundTriggerComponent>();
		if (soundTrigger != nullptr) {
			soundTrigger->DeactivateMusicCue(music.at(instrumentLot));
		}
	}

	self->CancelAllTimers();
	self->AddTimer("cleanupAfterStop", instrumentSmashAnimationTime.at(instrumentLot));
}

void NsConcertInstrument::EquipInstruments(Entity* self, Entity* player) {
	auto* inventory = player->GetComponent<InventoryComponent>();
	if (inventory != nullptr) {
		auto equippedItems = inventory->GetEquippedItems();

		// Un equip the current left item
		const auto equippedLeftItem = equippedItems.find("special_l");
		if (equippedLeftItem != equippedItems.end()) {
			auto* leftItem = inventory->FindItemById(equippedLeftItem->second.id);
			if (leftItem != nullptr) {
				leftItem->UnEquip();
				self->SetVar<LWOOBJID>(u"oldItemLeft", leftItem->GetId());
			}
		}

		// Un equip the current right item
		const auto equippedRightItem = equippedItems.find("special_r");
		if (equippedRightItem != equippedItems.end()) {
			auto* rightItem = inventory->FindItemById(equippedRightItem->second.id);
			if (rightItem != nullptr) {
				rightItem->UnEquip();
				self->SetVar<LWOOBJID>(u"oldItemRight", rightItem->GetId());
			}
		}

		// Equip the left hand instrument
		const auto leftInstrumentLot = instrumentLotLeft.find(GetInstrumentLot(self))->second;
		if (leftInstrumentLot != LOT_NULL) {
			inventory->AddItem(leftInstrumentLot, 1, eLootSourceType::LOOT_SOURCE_NONE, TEMP_ITEMS, {}, LWOOBJID_EMPTY, false);
			auto* leftInstrument = inventory->FindItemByLot(leftInstrumentLot, TEMP_ITEMS);
			leftInstrument->Equip();
		}

		// Equip the right hand instrument
		const auto rightInstrumentLot = instrumentLotRight.find(GetInstrumentLot(self))->second;
		if (rightInstrumentLot != LOT_NULL) {
			inventory->AddItem(rightInstrumentLot, 1, eLootSourceType::LOOT_SOURCE_NONE, TEMP_ITEMS, {}, LWOOBJID_EMPTY, false);
			auto* rightInstrument = inventory->FindItemByLot(rightInstrumentLot, TEMP_ITEMS);
			rightInstrument->Equip();
		}
	}
}

void NsConcertInstrument::UnEquipInstruments(Entity* self, Entity* player) {
	auto* inventory = player->GetComponent<InventoryComponent>();
	if (inventory != nullptr) {
		auto equippedItems = inventory->GetEquippedItems();

		// Un equip the current left instrument
		const auto equippedInstrumentLeft = equippedItems.find("special_l");
		if (equippedInstrumentLeft != equippedItems.end()) {
			auto* leftItem = inventory->FindItemById(equippedInstrumentLeft->second.id);
			if (leftItem != nullptr) {
				leftItem->UnEquip();
				inventory->RemoveItem(leftItem->GetLot(), 1, TEMP_ITEMS);
			}
		}

		// Un equip the current right instrument
		const auto equippedInstrumentRight = equippedItems.find("special_r");
		if (equippedInstrumentRight != equippedItems.end()) {
			auto* rightItem = inventory->FindItemById(equippedInstrumentRight->second.id);
			if (rightItem != nullptr) {
				rightItem->UnEquip();
				inventory->RemoveItem(rightItem->GetLot(), 1, TEMP_ITEMS);
			}
		}

		// Equip the old left hand item
		const auto leftItemID = self->GetVar<LWOOBJID>(u"oldItemLeft");
		if (leftItemID != LWOOBJID_EMPTY) {
			auto* item = inventory->FindItemById(leftItemID);
			if (item != nullptr)
				item->Equip();
			self->SetVar<LWOOBJID>(u"oldItemLeft", LWOOBJID_EMPTY);
		}

		// Equip the old right hand item
		const auto rightItemID = self->GetVar<LWOOBJID>(u"oldItemRight");
		if (rightItemID != LWOOBJID_EMPTY) {
			auto* item = inventory->FindItemById(rightItemID);
			if (item != nullptr)
				item->Equip();
			self->SetVar<LWOOBJID>(u"oldItemRight", LWOOBJID_EMPTY);
		}
	}
}

void NsConcertInstrument::RepositionPlayer(Entity* self, Entity* player) {
	auto position = self->GetPosition();
	auto rotation = self->GetRotation();
	position.SetY(0.0f);

	switch (GetInstrumentLot(self)) {
	case Bass:
	case Guitar:
		position.SetX(position.GetX() + 5.0f);
		break;
	case Keyboard:
		position.SetX(position.GetX() - 0.45f);
		position.SetZ(position.GetZ() + 0.75f);
		rotation = NiQuaternion::CreateFromAxisAngle(position, -0.8f); // Slight rotation to make the animation sensible
		break;
	case Drum:
		position.SetZ(position.GetZ() - 0.5f);
		break;
	}

	GameMessages::SendTeleport(player->GetObjectID(), position, rotation, player->GetSystemAddress());
}

InstrumentLot NsConcertInstrument::GetInstrumentLot(Entity* self) {
	return static_cast<const InstrumentLot>(self->GetLOT());
}

// Static stuff needed for script execution

const std::map<InstrumentLot, std::u16string> NsConcertInstrument::animations{
	{ Guitar, u"guitar"},
	{ Bass, u"bass"},
	{ Keyboard, u"keyboard"},
	{ Drum, u"drums"}
};

const std::map<InstrumentLot, std::u16string> NsConcertInstrument::smashAnimations{
	{Guitar, u"guitar-smash"},
	{Bass, u"bass-smash"},
	{Keyboard, u"keyboard-smash"},
	{Drum, u"keyboard-smash"}
};

const std::map<InstrumentLot, float> NsConcertInstrument::instrumentSmashAnimationTime{
		{Guitar, 2.167f},
		{Bass, 1.167f},
		{Keyboard, 1.0f},
		{Drum, 1.0f}
};

const std::map<InstrumentLot, std::string> NsConcertInstrument::music{
	{Guitar, "Concert_Guitar"},
	{Bass, "Concert_Bass"},
	{Keyboard, "Concert_Keys"},
	{Drum, "Concert_Drums"},
};

const std::map<InstrumentLot, std::u16string> NsConcertInstrument::cinematics{
	{Guitar, u"Concert_Cam_G"},
	{Bass, u"Concert_Cam_B"},
	{Keyboard, u"Concert_Cam_K"},
	{Drum, u"Concert_Cam_D"},
};

const std::map<InstrumentLot, LOT> NsConcertInstrument::instrumentLotLeft{
	{Guitar, 4991},
	{Bass, 4992},
	{Keyboard, LOT_NULL},
	{Drum, 4995},
};

const std::map<InstrumentLot, LOT> NsConcertInstrument::instrumentLotRight{
	{Guitar, LOT_NULL},
	{Bass, LOT_NULL},
	{Keyboard, LOT_NULL},
	{Drum, 4996},
};

const std::map<InstrumentLot, bool> NsConcertInstrument::hideInstrumentOnPlay{
	{Guitar, true},
	{Bass, true},
	{Keyboard, false},
	{Drum, false},
};

const std::map<InstrumentLot, float> NsConcertInstrument::instrumentEquipTime{
	{Guitar, 1.033},
	{Bass, 0.75},
	{Keyboard, -1},
	{Drum, 0},
};

const std::map<InstrumentLot, uint32_t> NsConcertInstrument::achievementTaskID{
		{Guitar, 911},
		{Bass, 912},
		{Keyboard, 913},
		{Drum, 914},
};

const uint32_t NsConcertInstrument::instrumentImaginationCost = 2;

const float NsConcertInstrument::instrumentCostFrequency = 4.0f;

const float NsConcertInstrument::updateFrequency = 1.0f;
