#include "ZoneAgProperty.h"
#include "EntityManager.h"
#include "Character.h"
#include "Entity.h"
#include "GameMessages.h"
#include "dZoneManager.h"
#include "RenderComponent.h"
#include "MissionComponent.h"

void ZoneAgProperty::SetGameVariables(Entity* self) {
	self->SetVar<std::string>(GuardGroup, "Guard");
	self->SetVar<std::string>(PropertyPlaqueGroup, "PropertyPlaque");
	self->SetVar<std::string>(PropertyVendorGroup, "PropertyVendor");
	self->SetVar<std::string>(PropertyBorderGroup, "PropertyBorder");
	self->SetVar<std::string>(LandTargetGroup, "Land_Target");
	self->SetVar<std::string>(SpiderScreamGroup, "Spider_Scream");
	self->SetVar<std::vector<std::string>>(ROFTargetsGroup, { "ROF_Targets_00", "ROF_Targets_01", "ROF_Targets_02", "ROF_Targets_03", "ROF_Targets_04" });
	self->SetVar<std::string>(SpiderEggsGroup, "SpiderEggs");
	self->SetVar<std::string>(RocksGroup, "Rocks");
	self->SetVar<std::string>(EnemiesGroup, "SpiderBoss");
	self->SetVar<std::vector<std::string>>(ZoneVolumesGroup, { "Zone1Vol", "Zone2Vol", "Zone3Vol", "Zone4Vol", "Zone5Vol", "Zone6Vol", "Zone7Vol", "Zone8Vol", "AggroVol", "TeleVol" });
	self->SetVar<std::string>(FXManagerGroup, "FXObject");

	self->SetVar<std::string>(EnemiesSpawner, "SpiderBoss");
	self->SetVar<std::vector<std::string>>(BossSensorSpawner, { "Zone1Vol", "Zone2Vol", "Zone3Vol", "Zone4Vol", "Zone5Vol", "Zone6Vol", "Zone7Vol", "Zone8Vol", "RFS_Targets", "AggroVol", "TeleVol" });
	self->SetVar<std::string>(LandTargetSpawner, "Land_Target");
	self->SetVar<std::string>(SpiderScreamSpawner, "Spider_Scream");
	self->SetVar<std::vector<std::string>>(ROFTargetsSpawner, { "ROF_Targets_00", "ROF_Targets_01", "ROF_Targets_02", "ROF_Targets_03", "ROF_Targets_04" });
	self->SetVar<std::string>(PropertyMGSpawner, "PropertyGuard");
	self->SetVar<std::string>(FXManagerSpawner, "FXObject");
	self->SetVar<std::string>(PropObjsSpawner, "BankObj");
	self->SetVar<std::string>(SpiderEggsSpawner, "SpiderEggs");
	self->SetVar<std::string>(RocksSpawner, "Rocks");
	self->SetVar<std::vector<std::string>>(AmbientFXSpawner, { "BirdFX", "SunBeam" });
	self->SetVar<std::vector<std::string>>(SpiderRocketSpawner, { "SpiderRocket_Bot", "SpiderRocket_Mid", "SpiderRocket_Top" });
	self->SetVar<std::string>(MailboxSpawner, "Mailbox");
	self->SetVar<std::string>(LauncherSpawner, "Launcher");
	self->SetVar<std::string>(InstancerSpawner, "Instancer");

	self->SetVar<uint32_t>(defeatedProperyFlag, 71);
	self->SetVar<uint32_t>(placedModelFlag, 73);
	self->SetVar<uint32_t>(guardFirstMissionFlag, 891);
	self->SetVar<uint32_t>(guardMissionFlag, 320);
	self->SetVar<uint32_t>(brickLinkMissionIDFlag, 951);
}

void ZoneAgProperty::OnStartup(Entity* self) {
	LoadProperty(self);
}

void ZoneAgProperty::OnPlayerLoaded(Entity* self, Entity* player) {
	CheckForOwner(self);

	auto rented = self->GetVar<LWOOBJID>(u"PropertyOwner") == LWOOBJID_EMPTY;
	self->SetVar<bool>(u"rented", rented);

	if (!rented) {
		const auto numberOfPlayers = self->GetVar<int32_t>(u"numberOfPlayers");
		self->SetVar<int32_t>(u"numberOfPlayers", numberOfPlayers + 1);
	}

	if (dZoneManager::Instance()->GetZone()->GetZoneID().GetMapID() == 1102) {
		GameMessages::SendPlay2DAmbientSound(player, GUIDMaelstrom);
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"maelstromSkyOn", 0, 0,
			LWOOBJID_EMPTY, "", player->GetSystemAddress());

		self->SetNetworkVar(u"unclaimed", true);

		return;
	}

	BasePlayerLoaded(self, player);
}

void ZoneAgProperty::PropGuardCheck(Entity* self, Entity* player) {
	auto* missionComponent = player->GetComponent<MissionComponent>();
	if (missionComponent == nullptr)
		return;

	const auto state = missionComponent->GetMissionState(self->GetVar<uint32_t>(guardMissionFlag));
	const auto firstState = missionComponent->GetMissionState(self->GetVar<uint32_t>(guardFirstMissionFlag));

	if (firstState < MissionState::MISSION_STATE_COMPLETE || (state != MissionState::MISSION_STATE_COMPLETE && state != MissionState::MISSION_STATE_COMPLETE_READY_TO_COMPLETE))
		ActivateSpawner(self->GetVar<std::string>(PropertyMGSpawner));
}

void ZoneAgProperty::OnZoneLoadedInfo(Entity* self) {
	LoadProperty(self);
}

void ZoneAgProperty::LoadInstance(Entity* self) {
	SetGameVariables(self);

	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(self->GetVar<std::string>(InstancerSpawner))) {
		for (auto* spawnerNode : spawner->m_Info.nodes) {
			spawnerNode->config.push_back(
				new LDFData<std::string>(u"custom_script_server",
					R"(scripts\ai\GENERAL\L_INSTANCE_EXIT_TRANSFER_PLAYER_TO_LAST_NON_INSTANCE.lua)"));
			spawnerNode->config.push_back(new LDFData<std::u16string>(u"transferText", u"SPIDER_QUEEN_EXIT_QUESTION"));
		}
	}

	ActivateSpawner(self->GetVar<std::string>(InstancerSpawner));
}

void ZoneAgProperty::LoadProperty(Entity* self) {
	SetGameVariables(self);
	ActivateSpawner(self->GetVar<std::string>(LauncherSpawner));
	ActivateSpawner(self->GetVar<std::string>(MailboxSpawner));
}

void ZoneAgProperty::ProcessGroupObjects(Entity* self, std::string group) {
}

void ZoneAgProperty::SpawnSpots(Entity* self) {
	for (const auto& spot : self->GetVar<std::vector<std::string>>(ROFTargetsSpawner)) {
		ActivateSpawner(spot);
	}

	ActivateSpawner(self->GetVar<std::string>(LandTargetSpawner));
}

void ZoneAgProperty::KillSpots(Entity* self) {
	for (const auto& spot : self->GetVar<std::vector<std::string>>(ROFTargetsSpawner)) {
		DeactivateSpawner(spot);
	}

	for (const auto& groupName : self->GetVar<std::vector<std::string>>(ROFTargetsGroup)) {
		for (auto* spot : EntityManager::Instance()->GetEntitiesInGroup(groupName)) {
			spot->Kill();
		}
	}

	DeactivateSpawner(self->GetVar<std::string>(LandTargetSpawner));
	for (auto* landTarget : EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(LandTargetSpawner))) {
		landTarget->Kill();
	}
}

void ZoneAgProperty::SpawnCrashedRocket(Entity* self) {
	for (const auto& rocket : self->GetVar<std::vector<std::string>>(SpiderRocketSpawner)) {
		ActivateSpawner(rocket);
	}
}

void ZoneAgProperty::KillCrashedRocket(Entity* self) {
	for (const auto& rocket : self->GetVar<std::vector<std::string>>(SpiderRocketSpawner)) {
		DeactivateSpawner(rocket);
		DestroySpawner(rocket);
	}
}

void ZoneAgProperty::StartMaelstrom(Entity* self, Entity* player) {
	ActivateSpawner(self->GetVar<std::string>(EnemiesSpawner));
	for (const auto& sensor : self->GetVar<std::vector<std::string>>(BossSensorSpawner)) {
		ActivateSpawner(sensor);
	}

	ActivateSpawner(self->GetVar<std::string>(FXManagerSpawner));
	ActivateSpawner(self->GetVar<std::string>(SpiderScreamSpawner));
	ActivateSpawner(self->GetVar<std::string>(SpiderEggsSpawner));
	ActivateSpawner(self->GetVar<std::string>(RocksSpawner));

	SpawnCrashedRocket(self);

	for (const auto& ambient : self->GetVar<std::vector<std::string>>(AmbientFXSpawner)) {
		DeactivateSpawner(ambient);
		DestroySpawner(ambient);
		ResetSpawner(ambient);
	}

	StartTornadoFx(self);

	if (player != nullptr) {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"maelstromSkyOn", 0, 0, LWOOBJID_EMPTY,
			"", player->GetSystemAddress());
	}
}

uint32_t ZoneAgProperty::RetrieveSpawnerId(Entity* self, const std::string& spawner) {
	auto spawnerIDs = dZoneManager::Instance()->GetSpawnersByName(spawner);
	if (spawnerIDs.empty())
		return 0;

	return spawnerIDs[0]->m_Info.spawnerID;
}

void ZoneAgProperty::OnTimerDone(Entity* self, std::string timerName) {
	BaseTimerDone(self, timerName);
}

void ZoneAgProperty::BaseTimerDone(Entity* self, const std::string& timerName) {
	if (timerName == "GuardFlyAway") {
		const auto zoneId = dZoneManager::Instance()->GetZone()->GetWorldID();
		if (zoneId != 1150)
			return;

		const auto entities = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(GuardGroup));
		if (entities.empty())
			return;

		auto* entity = entities[0];

		GameMessages::SendNotifyClientObject(EntityManager::Instance()->GetZoneControlEntity()->GetObjectID(), u"GuardChat", 0, 0, entity->GetObjectID(), "", UNASSIGNED_SYSTEM_ADDRESS);
		LoadProperty(self);

		self->AddTimer("KillGuard", 5);
	} else if (timerName == "KillGuard") {
		KillGuard(self);
	} else if (timerName == "tornadoOff") {
		for (auto* entity : EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup))) {
			auto* renderComponent = entity->GetComponent<RenderComponent>();
			if (renderComponent != nullptr) {
				renderComponent->StopEffect("TornadoDebris", false);
				renderComponent->StopEffect("TornadoVortex", false);
				renderComponent->StopEffect("silhouette", false);
			}
		}

		self->AddTimer("ShowVendor", 1.2f);
		self->AddTimer("ShowClearEffects", 2);
	} else if (timerName == "ShowClearEffects") {
		for (auto* entity : EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup))) {
			auto* renderComponent = entity->GetComponent<RenderComponent>();
			if (renderComponent != nullptr) {
				renderComponent->PlayEffect(-1, u"beamOn", "beam");
			}
		}

		self->AddTimer("killSpider", 2);
		self->AddTimer("turnSkyOff", 1.5f);
		self->AddTimer("killFXObject", 8);
	} else if (timerName == "turnSkyOff") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"SkyOff", 0, 0, LWOOBJID_EMPTY,
			"", UNASSIGNED_SYSTEM_ADDRESS);
	} else if (timerName == "killSpider") {
		for (auto* entity : EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(EnemiesGroup))) {
			entity->Kill();
		}

		for (const auto& sensor : self->GetVar<std::vector<std::string>>(BossSensorSpawner)) {
			DeactivateSpawner(sensor);
			DestroySpawner(sensor);
		}

		DeactivateSpawner(self->GetVar<std::string>(SpiderEggsSpawner));
		DestroySpawner(self->GetVar<std::string>(SpiderEggsSpawner));

		DeactivateSpawner(self->GetVar<std::string>(RocksSpawner));
		DestroySpawner(self->GetVar<std::string>(RocksSpawner));

		KillSpots(self);
		KillCrashedRocket(self);

		DeactivateSpawner(self->GetVar<std::string>(SpiderScreamSpawner));
		DestroySpawner(self->GetVar<std::string>(SpiderScreamSpawner));

		for (auto* player : EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_CHARACTER)) {
			GameMessages::SendStop2DAmbientSound(player, true, GUIDMaelstrom);
			GameMessages::SendPlay2DAmbientSound(player, GUIDPeaceful);
		}
	} else if (timerName == "ShowVendor") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"vendorOn", 0, 0, LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);
		for (const auto& ambient : self->GetVar<std::vector<std::string>>(AmbientFXSpawner)) {
			ActivateSpawner(ambient);
		}
	} else if (timerName == "BoundsVisOn") {
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"boundsAnim", 0, 0,
			LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);
	} else if (timerName == "runPlayerLoadedAgain") {
		CheckForOwner(self);
	} else if (timerName == "pollTornadoFX") {
		StartTornadoFx(self);
	} else if (timerName == "killFXObject") {
		for (auto* entity : EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup))) {
			auto* renderComponent = entity->GetComponent<RenderComponent>();
			if (renderComponent != nullptr) {
				renderComponent->StopEffect("beam");
			}
		}

		DestroySpawner(self->GetVar<std::string>(FXManagerSpawner));

		self->SetVar<bool>(u"FXObjectGone", true);
	} else if (timerName == "ProcessGroupObj") {
		// TODO
	}
}

void ZoneAgProperty::OnZonePropertyRented(Entity* self, Entity* player) {
	BaseZonePropertyRented(self, player);

	auto* character = player->GetCharacter();
	if (character == nullptr)
		return;

	character->SetPlayerFlag(108, true);
}

void ZoneAgProperty::OnZonePropertyModelPlaced(Entity* self, Entity* player) {
	auto* character = player->GetCharacter();
	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (!character->GetPlayerFlag(101)) {
		BaseZonePropertyModelPlaced(self, player);
		character->SetPlayerFlag(101, true);
		if (missionComponent->GetMissionState(871) == MissionState::MISSION_STATE_ACTIVE) {
			self->SetNetworkVar<std::u16string>(u"Tooltip", u"AnotherModel");
		}

	} else if (!character->GetPlayerFlag(102)) {
		character->SetPlayerFlag(102, true);
		if (missionComponent->GetMissionState(871) == MissionState::MISSION_STATE_ACTIVE) {
			self->SetNetworkVar<std::u16string>(u"Tooltip", u"TwoMoreModels");
		}

	} else if (!character->GetPlayerFlag(103)) {
		character->SetPlayerFlag(103, true);
	} else if (!character->GetPlayerFlag(104)) {
		character->SetPlayerFlag(104, true);
		self->SetNetworkVar<std::u16string>(u"Tooltip", u"TwoMoreModelsOff");
	} else if (self->GetVar<std::string>(u"tutorial") == "place_model") {
		self->SetVar<std::string>(u"tutorial", "");
		self->SetNetworkVar<std::u16string>(u"Tooltip", u"PutAway");
	}
}

void ZoneAgProperty::OnZonePropertyModelPickedUp(Entity* self, Entity* player) {
	auto* character = player->GetCharacter();
	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (!character->GetPlayerFlag(109)) {
		character->SetPlayerFlag(109, true);
		if (missionComponent->GetMissionState(891) == MissionState::MISSION_STATE_ACTIVE && !character->GetPlayerFlag(110)) {
			self->SetNetworkVar<std::u16string>(u"Tooltip", u"Rotate");
		}
	}
}

void ZoneAgProperty::OnZonePropertyModelRemoved(Entity* self, Entity* player) {
	auto* character = player->GetCharacter();
	character->SetPlayerFlag(111, true);
}

void ZoneAgProperty::OnZonePropertyModelRemovedWhileEquipped(Entity* self, Entity* player) {
	ZoneAgProperty::OnZonePropertyModelRemoved(self, player);
}

void ZoneAgProperty::OnZonePropertyModelRotated(Entity* self, Entity* player) {
	auto* character = player->GetCharacter();
	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (!character->GetPlayerFlag(110)) {
		character->SetPlayerFlag(110, true);

		if (missionComponent->GetMissionState(891) == MissionState::MISSION_STATE_ACTIVE) {
			self->SetNetworkVar<std::u16string>(u"Tooltip", u"PlaceModel");
			self->SetVar<std::string>(u"tutorial", "place_model");
		}
	}
}

void ZoneAgProperty::OnZonePropertyModelEquipped(Entity* self) {
	self->SetNetworkVar<std::u16string>(u"PlayerAction", u"ModelEquipped");
}

void ZoneAgProperty::OnZonePropertyEditBegin(Entity* self) {
	self->SetNetworkVar<std::u16string>(u"PlayerAction", u"Enter");
}

void ZoneAgProperty::OnZonePropertyEditEnd(Entity* self) {
	self->SetNetworkVar<std::u16string>(u"PlayerAction", u"Exit");
}

void ZoneAgProperty::OnPlayerExit(Entity* self) {
	// TODO: Destroy stuff
}

void ZoneAgProperty::RemovePlayerRef(Entity* self) {
	// TODO: Destroy stuff
}

void ZoneAgProperty::BaseOnFireEventServerSide(Entity* self, Entity* sender, std::string args) {
	if (args == "propertyRented") {
		const auto playerId = self->GetVar<LWOOBJID>(u"playerID");
		auto* player = EntityManager::Instance()->GetEntity(playerId);
		if (player == nullptr)
			return;

		OnZonePropertyRented(self, player);
	} else if (args == "RetrieveZoneData") {
		self->SetVar<LWOOBJID>(u"SpiderBossID", sender->GetObjectID());
		sender->SetVar<int32_t>(u"SpiderEggNetworkID", RetrieveSpawnerId(self, self->GetVar<std::string>(SpiderEggsSpawner)));

		std::vector<uint32_t> table;

		for (const auto& target : self->GetVar<std::vector<std::string>>(ROFTargetsSpawner)) {
			table.push_back(RetrieveSpawnerId(self, target));
		}

		ROFTargetGroupIdTable = table;

		ProcessGroupObjects(self, self->GetVar<std::string>(LandTargetGroup));
		ProcessGroupObjects(self, self->GetVar<std::string>(SpiderScreamGroup));
		//        ProcessGroupObjects(self, groups.ZoneVolumes);
	} else if (args == "CheckForPropertyOwner") {
		sender->SetNetworkVar<std::string>(u"PropertyOwnerID", std::to_string(self->GetVar<LWOOBJID>(u"PropertyOwner")));
	} else if (args == "ClearProperty") {
		const auto playerId = self->GetVar<LWOOBJID>(u"playerID");
		auto* player = EntityManager::Instance()->GetEntity(playerId);
		if (player == nullptr)
			return;

		player->GetCharacter()->SetPlayerFlag(self->GetVar<uint32_t>(defeatedProperyFlag), true);
		GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlayCinematic", 0, 0,
			LWOOBJID_EMPTY, destroyedCinematic, UNASSIGNED_SYSTEM_ADDRESS);

		self->AddTimer("tornadoOff", 0.5f);
	}
}

void ZoneAgProperty::NotifyDie(Entity* self) {
	// TODO
}
