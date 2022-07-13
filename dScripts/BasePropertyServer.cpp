#include "BasePropertyServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Character.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "RenderComponent.h"
#include "PropertyManagementComponent.h"
#include "MissionComponent.h"

void BasePropertyServer::SetGameVariables(Entity *self) {
    self->SetVar<std::string>(ClaimMarkerGroup, "");
    self->SetVar<std::string>(GeneratorGroup, "");
    self->SetVar<std::string>(GuardGroup, "");
    self->SetVar<std::string>(PropertyPlaqueGroup, "");
    self->SetVar<std::string>(PropertyVendorGroup, "");
    self->SetVar<std::string>(SpotsGroup, "");
    self->SetVar<std::string>(MSCloudsGroup, "");
    self->SetVar<std::string>(EnemiesGroup, "");
    self->SetVar<std::string>(FXManagerGroup, "");
    self->SetVar<std::string>(ImagOrbGroup, "");
    self->SetVar<std::string>(GeneratorFXGroup, "");

    self->SetVar<std::vector<std::string>>(EnemiesSpawner, {});
    self->SetVar<std::string>(ClaimMarkerSpawner, "");
    self->SetVar<std::string>(GeneratorSpawner, "");
    self->SetVar<std::string>(DamageFXSpawner, "");
    self->SetVar<std::string>(FXSpotsSpawner, "");
    self->SetVar<std::string>(PropertyMGSpawner, "");
    self->SetVar<std::string>(ImageOrbSpawner, "");
    self->SetVar<std::string>(GeneratorFXSpawner, "");
    self->SetVar<std::string>(SmashablesSpawner, "");
    self->SetVar<std::string>(FXManagerSpawner, "");
    self->SetVar<std::string>(PropObjsSpawner, "");
    self->SetVar<std::vector<std::string>>(AmbientFXSpawner, {});
    self->SetVar<std::vector<std::string>>(BehaviorObjsSpawner, {});

    self->SetVar<uint32_t>(defeatedProperyFlag, 0);
    self->SetVar<uint32_t>(placedModelFlag, 0);
    self->SetVar<uint32_t>(guardMissionFlag, 0);
    self->SetVar<uint32_t>(brickLinkMissionIDFlag, 0);
    self->SetVar<std::string>(passwordFlag, "s3kratK1ttN");
    self->SetVar<LOT>(generatorIdFlag, 0);
    self->SetVar<LOT>(orbIDFlag, 0);
    self->SetVar<LOT>(behaviorQBID, 0);
}

void BasePropertyServer::CheckForOwner(Entity* self) {
    if (EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(PropertyPlaqueGroup)).empty()) {
        self->AddTimer(RunPlayerLoadedAgainTimer, 0.5f);
        return;
    }

	self->SetI64(PropertyOwnerVariable, GetOwner());
}

void BasePropertyServer::OnStartup(Entity *self) {
    SetGameVariables(self);
}

void BasePropertyServer::OnFireEventServerSide(Entity *self, Entity *sender, std::string args, int32_t param1,
                                               int32_t param2, int32_t param3) {
    if (args == CheckForPropertyOwnerEvent) {
        sender->SetNetworkVar<std::string>(PropertyOwnerIDVariable, std::to_string(self->GetVar<LWOOBJID>(PropertyOwnerVariable)));
    }
}

void BasePropertyServer::BasePlayerLoaded(Entity* self, Entity* player) {
	CheckForOwner(self);

	auto rented = false;
	auto propertyOwner = PropertyManagementComponent::Instance()->GetOwnerId();

	self->OnFireEventServerSide(self, CheckForPropertyOwnerEvent);
	
	if (propertyOwner > 0) {
		rented = true;
	}

	if (propertyOwner < 0) {
		propertyOwner = LWOOBJID_EMPTY;
	}

	self->SetNetworkVar(PropertyOwnerIDVariable, propertyOwner);

	if (rented) {
        auto plaques = EntityManager::Instance()->GetEntitiesInGroup("PropertyVendor");
        for (auto* plaque : plaques) {
            EntityManager::Instance()->DestructEntity(plaque);
        }

		const auto& mapID = dZoneManager::Instance()->GetZone()->GetZoneID();

		if (propertyOwner > 0)
		{
			auto* missionComponent = player->GetComponent<MissionComponent>();

			if (missionComponent != nullptr)
			{	
				missionComponent->Progress(
					MissionTaskType::MISSION_TASK_TYPE_VISIT_PROPERTY,
					mapID.GetMapID(),
					mapID.GetCloneID()
				);
			}
		}

		GameMessages::SendStop2DAmbientSound(player, true, GUIDMaelstrom);
		GameMessages::SendPlay2DAmbientSound(player, GUIDPeaceful);

		self->AddTimer(TurnSkyOffTimer, 1.5f);

		// kill tornado FX and play peaceful noises
		if (!self->GetBoolean(FXObjectsGoneVariable)) {
			self->AddTimer(KillFXObjectTimer, 1.0f);
		}
		
		GameMessages::SendPlay2DAmbientSound(player, GUIDPeaceful);

		// activate property safe spawner network
		ActivateSpawner(self->GetVar<std::string>(PropObjsSpawner));

		// tell client script who owns the property
		self->SetNetworkVar<std::string>(RenterVariable, std::to_string(propertyOwner));

		if (player->GetObjectID() != propertyOwner)
			return;
	} else {
		const auto defeatedFlag = player->GetCharacter()->GetPlayerFlag(self->GetVar<uint32_t>(defeatedProperyFlag));

		self->SetNetworkVar(UnclaimedVariable, true);
        self->SetVar<LWOOBJID>(PlayerIDVariable, player->GetObjectID());

		if (!defeatedFlag) {
			StartMaelstrom(self, player);
			SpawnSpots(self);
			GameMessages::SendPlay2DAmbientSound(player, GUIDMaelstrom);
		} else {
			GameMessages::SendStop2DAmbientSound(player, true, GUIDMaelstrom);
			GameMessages::SendPlay2DAmbientSound(player, GUIDPeaceful);

			self->AddTimer(TurnSkyOffTimer, 1.5f);
			self->AddTimer(KillFXObjectTimer, 1.0f);
		}
	}

	PropGuardCheck(self, player);
}

void BasePropertyServer::PropGuardCheck(Entity* self, Entity* player) {
	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent != nullptr
	&& missionComponent->GetMissionState(self->GetVar<uint32_t>(guardMissionFlag)) != MissionState::MISSION_STATE_COMPLETE) {
	    ActivateSpawner(self->GetVar<std::string>(PropertyMGSpawner));
	}
}

void BasePropertyServer::BaseZonePropertyRented(Entity* self, Entity* player) const {
	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"PlayCinematic", 0, 0, LWOOBJID_EMPTY, "ShowProperty",
                                      UNASSIGNED_SYSTEM_ADDRESS);

	self->AddTimer(BoundsVisOnTimer, 2);
	self->SetVar<LWOOBJID>(PropertyOwnerVariable, player->GetObjectID());
    
    auto plaques = EntityManager::Instance()->GetEntitiesInGroup("PropertyVendor");
    for (auto* plaque : plaques) {
        EntityManager::Instance()->DestructEntity(plaque);
    }

    auto brickLinkMissionID = self->GetVar<uint32_t>(brickLinkMissionIDFlag);
	if (brickLinkMissionID != 0) {
        auto missionComponent = player->GetComponent<MissionComponent>();
        if (missionComponent) missionComponent->CompleteMission(brickLinkMissionID, true);
	}

	ActivateSpawner(self->GetVar<std::string>(PropObjsSpawner));
}

void BasePropertyServer::BaseZonePropertyModelPlaced(Entity* self, Entity* player) const {
	auto* character = player->GetCharacter();
	if (character == nullptr)
		return;

	auto flag = self->GetVar<int32_t>(placedModelFlag);
	if (flag)
	    character->SetPlayerFlag(flag, true);
}

void BasePropertyServer::KillClouds(Entity* self) {
	DeactivateSpawner(self->GetVar<std::string>(DamageFXSpawner));
	DestroySpawner(self->GetVar<std::string>(DamageFXSpawner));
}

void BasePropertyServer::SpawnSpots(Entity* self) {
	ActivateSpawner(self->GetVar<std::string>(FXSpotsSpawner));
}

void BasePropertyServer::KillSpots(Entity* self) {
	DeactivateSpawner(self->GetVar<std::string>(FXSpotsSpawner));
	DestroySpawner(self->GetVar<std::string>(FXSpotsSpawner));
}

void BasePropertyServer::StartMaelstrom(Entity* self, Entity* player) {
    for (const auto& enemySpawner : self->GetVar<std::vector<std::string>>(EnemiesSpawner)) {
        ActivateSpawner(enemySpawner);
    }

    for (const auto& behaviorObjectSpawner : self->GetVar<std::vector<std::string>>(BehaviorObjsSpawner)) {
        ActivateSpawner(behaviorObjectSpawner);
    }

	ActivateSpawner(self->GetVar<std::string>(DamageFXSpawner));
	ActivateSpawner(self->GetVar<std::string>(GeneratorSpawner));
	ActivateSpawner(self->GetVar<std::string>(GeneratorFXSpawner));
	ActivateSpawner(self->GetVar<std::string>(FXManagerSpawner));
	ActivateSpawner(self->GetVar<std::string>(ImageOrbSpawner));
	ActivateSpawner(self->GetVar<std::string>(SmashablesSpawner));

	DestroySpawner(self->GetVar<std::string>(ClaimMarkerSpawner));

	for (const auto& ambientFXSpawner : self->GetVar<std::vector<std::string>>(AmbientFXSpawner)) {
	    DestroySpawner(ambientFXSpawner);
	}

	StartTornadoFx(self);

	GameMessages::SendNotifyClientObject(self->GetObjectID(), u"maelstromSkyOn", 0, 0, LWOOBJID_EMPTY,
                                      "", player->GetSystemAddress());

	self->AddTimer(StartGeneratorTimer, 0.0f);
	self->AddTimer(StartOrbTimer, 0.0f);
}

void BasePropertyServer::StartTornadoFx(Entity* self) const {
	const auto entities = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup));
	if (entities.empty()) {
		self->AddTimer("pollTornadoFX", 0.1f);
		return;
	}

	for (auto* entity : entities) {
	    auto* renderComponent = entity->GetComponent<RenderComponent>();
	    if (renderComponent != nullptr) {
	        renderComponent->PlayEffect(-1, u"debrisOn", "TornadoDebris");
	        renderComponent->PlayEffect(-1, u"VortexOn", "TornadoVortex");
	        renderComponent->PlayEffect(-1, u"onSilhouette", "silhouette");
	    }
	}
}

void BasePropertyServer::BasePlayerExit(Entity *self, Entity *player) {
	if (self->GetBoolean(UnclaimedVariable)) {
	    if (player->GetObjectID() == self->GetVar<LWOOBJID>(PlayerIDVariable)) {
	        // Destroy all spawners
	    }
	}
}

void BasePropertyServer::KillGuard(Entity* self) {
	const auto entities = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(GuardGroup));
	if (entities.empty())
		return;

	DeactivateSpawner(self->GetVar<std::string>(PropertyMGSpawner));

	auto* guard = entities[0];
	guard->Smash(self->GetObjectID());
}

void BasePropertyServer::RequestDie(Entity* self, Entity* other) {
	auto* destroyable = other->GetComponent<DestroyableComponent>();
	if (destroyable == nullptr)
		return;

	destroyable->Smash(other->GetObjectID(), SILENT);
}

void BasePropertyServer::ActivateSpawner(const std::string& spawnerName) {
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(spawnerName)) {
		spawner->Activate();
	}
}

void BasePropertyServer::DeactivateSpawner(const std::string& spawnerName) {
    for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(spawnerName)) {
        spawner->Deactivate();
    }
}

void BasePropertyServer::TriggerSpawner(const std::string& spawnerName) {
	for (auto* spawner : dZoneManager::Instance()->GetSpawnersInGroup(spawnerName)) {
		spawner->Spawn();
	}
}

void BasePropertyServer::ResetSpawner(const std::string& spawnerName) {
    for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(spawnerName)) {
        spawner->Reset();
    }
}

void BasePropertyServer::DestroySpawner(const std::string& spawnerName) {
    for (auto* spawner : dZoneManager::Instance()->GetSpawnersByName(spawnerName)) {
        for (auto* node : spawner->m_Info.nodes) {
            for (const auto& element : node->entities) {
                auto* entity = EntityManager::Instance()->GetEntity(element);
                if (entity == nullptr)
                    continue;

                entity->Kill();
            }

            node->entities.clear();
        }

        spawner->Deactivate();
    }
}

LWOOBJID BasePropertyServer::GetOwner() {
	auto* manager = PropertyManagementComponent::Instance();
	return manager == nullptr ? LWOOBJID_EMPTY : manager->GetOwnerId();
}

void BasePropertyServer::BaseTimerDone(Entity* self, const std::string& timerName) {
    if (timerName == StartGeneratorTimer) {
        HandleGeneratorTimer(self);
    } else if (timerName == StartOrbTimer) {
        HandleOrbsTimer(self);
    } else if (timerName == StartQuickbuildTimer) {
        HandleQuickBuildTimer(self);
    } else if (timerName == "GuardFlyAway") {
        const auto zoneId = dZoneManager::Instance()->GetZone()->GetWorldID();

        // No guard for the spider instance fight
        if (dZoneManager::Instance()->GetZoneID().GetMapID() == 1150)
            return;

        const auto entities = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(GuardGroup));
        if (entities.empty())
            return;

        auto* guard = entities[0];
        GameMessages::SendNotifyClientObject(EntityManager::Instance()->GetZoneControlEntity()->GetObjectID(),
                                             u"GuardChat", 0, 0, guard->GetObjectID(),
                                             "", UNASSIGNED_SYSTEM_ADDRESS);

        self->AddTimer(KillGuardTimer, 5.0f);
    } else if (timerName == KillGuardTimer) {
        KillGuard(self);
    } else if (timerName == TornadoOffTimer) {
        auto fxManagers = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup));

        for (auto *fxManager : fxManagers) {
            auto *renderComponent = fxManager->GetComponent<RenderComponent>();
            if (renderComponent != nullptr) {
                renderComponent->StopEffect("TornadoDebris", false);
                renderComponent->StopEffect("TornadoVortex", false);
                renderComponent->StopEffect("silhouette", false);
            }
        }

        self->AddTimer(ShowClearEffectsTimer, 2);
    } else if (timerName == ShowClearEffectsTimer) {
        auto fxManagers = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup));

        for (auto *fxManager : fxManagers) {
            auto *renderComponent = fxManager->GetComponent<RenderComponent>();
            if (renderComponent != nullptr)
                renderComponent->PlayEffect(-1, u"beamOn", "beam");
        }

        self->AddTimer(KillStrombiesTimer, 2.0f);
        self->AddTimer(TurnSkyOffTimer, 1.5f);
        self->AddTimer(KillFXObjectTimer, 8.0f);
    } else if (timerName == TurnSkyOffTimer) {
        auto* controller = dZoneManager::Instance()->GetZoneControlObject();
        GameMessages::SendNotifyClientObject(controller->GetObjectID(), u"SkyOff", 0, 0,
                                             LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);
    } else if (timerName == KillStrombiesTimer) {
        const auto enemies = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(EnemiesGroup));
        for (auto* enemy : enemies) {
            RequestDie(self, enemy);
        }

        DestroySpawner(self->GetVar<std::string>(SmashablesSpawner));
        KillSpots(self);

        auto* player = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
        if (player == nullptr)
            return;

        GameMessages::SendStop2DAmbientSound(player, true, GUIDMaelstrom);
        GameMessages::SendPlay2DAmbientSound(player, GUIDPeaceful);

        self->AddTimer(ShowVendorTimer, 5.0f);
    } else if (timerName == KillMarkerTimer) {
        DestroySpawner(self->GetVar<std::string>(ClaimMarkerSpawner));

        for (const auto& behaviorObjectSpawner : self->GetVar<std::vector<std::string>>(BehaviorObjsSpawner)) {
            DestroySpawner(behaviorObjectSpawner);
        }

        for (auto* entity : EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(ImagOrbGroup))) {
            entity->Smash();
        }

        DestroySpawner(self->GetVar<std::string>(ImageOrbSpawner));

        self->AddTimer(ShowVendorTimer, 1.0f);
    } else if (timerName == ShowVendorTimer) {
        GameMessages::SendNotifyClientObject(EntityManager::Instance()->GetZoneControlEntity()->GetObjectID(),
                                             u"vendorOn", 0, 0, LWOOBJID_EMPTY, "",
                                             UNASSIGNED_SYSTEM_ADDRESS);

        for (const auto& ambientFXSpawner : self->GetVar<std::vector<std::string>>(AmbientFXSpawner)) {
            ActivateSpawner(ambientFXSpawner);
        }
    } else if (timerName == BoundsVisOnTimer) {
        GameMessages::SendNotifyClientObject(self->GetObjectID(), u"boundsAnim", 0, 0,
                                             LWOOBJID_EMPTY, "", UNASSIGNED_SYSTEM_ADDRESS);
    } else if (timerName == RunPlayerLoadedAgainTimer) {
        CheckForOwner(self);
    } else if (timerName == PollTornadoFXTimer) {
        StartTornadoFx(self);
    } else if (timerName == KillFXObjectTimer) {
        const auto fxManagers = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(FXManagerGroup));
        if (fxManagers.empty()) {
            self->AddTimer(KillFXObjectTimer, 1.0f);
            return;
        }

        for (auto* fxManager : fxManagers) {
            auto* renderComponent = fxManager->GetComponent<RenderComponent>();
            if (renderComponent != nullptr) {
                renderComponent->StopEffect("beam");
            }
        }

        DestroySpawner(self->GetVar<std::string>(FXManagerSpawner));
        self->SetVar<bool>(u"FXObjectGone", true);
    }
}

void BasePropertyServer::HandleOrbsTimer(Entity* self) {
    self->SetVar<bool>(CollidedVariable, false);
    auto orbs = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(ImagOrbGroup));
    if (orbs.empty()) {
        self->AddTimer(StartOrbTimer, 0.5f);
        return;
    }

    for (auto* orb : orbs) {
        orb->AddCollisionPhantomCallback([self, this](Entity* other) {
            if (other != nullptr && other->IsPlayer() && !self->GetVar<bool>(CollidedVariable)) {
                self->SetVar<bool>(CollidedVariable, true);

                KillClouds(self);
                DeactivateSpawner(self->GetVar<std::string>(GeneratorSpawner));

                for (const auto& enemySpawner : self->GetVar<std::vector<std::string>>(EnemiesSpawner)) {
                    DeactivateSpawner(enemySpawner);
                }

                DestroySpawner(self->GetVar<std::string>(GeneratorFXSpawner));
                GameMessages::SendNotifyClientObject(EntityManager::Instance()->GetZoneControlEntity()->GetObjectID(),
                                                     u"PlayCinematic", 0, 0, LWOOBJID_EMPTY,
                                                     "DestroyMaelstrom", UNASSIGNED_SYSTEM_ADDRESS);

                // Notifies the client that the property has been claimed with a flag, completes missions too
                auto* player = EntityManager::Instance()->GetEntity(self->GetVar<LWOOBJID>(PlayerIDVariable));
                if (player != nullptr) {
                    auto* character = player->GetCharacter();
                    if (character != nullptr) {
                        character->SetPlayerFlag(self->GetVar<uint32_t>(defeatedProperyFlag), true);
                    }
                }

                self->AddTimer(TornadoOffTimer, 0.5f);
                self->AddTimer(KillMarkerTimer, 0.7f);
            }
        });
    }
}

void BasePropertyServer::HandleGeneratorTimer(Entity* self) {
    auto generators = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(GeneratorGroup));
    if (generators.empty()) {
        self->AddTimer(StartGeneratorTimer, 0.5f);
        return;
    }

    for (auto* generator : generators) {
        generator->AddDieCallback([self, this]() {
            ActivateSpawner(self->GetVar<std::string>(ClaimMarkerSpawner));
            self->AddTimer(StartQuickbuildTimer, 0.0f);

            for (const auto& enemySpawner : self->GetVar<std::vector<std::string>>(EnemiesSpawner)) {
                DeactivateSpawner(enemySpawner);
            }
            DeactivateSpawner(self->GetVar<std::string>(GeneratorSpawner));
        });
    }
}

void BasePropertyServer::HandleQuickBuildTimer(Entity* self) {
    auto claimMarkers = EntityManager::Instance()->GetEntitiesInGroup(self->GetVar<std::string>(ClaimMarkerGroup));
    if (claimMarkers.empty()) {
        self->AddTimer(StartQuickbuildTimer, 0.5f);
        return;
    }

    for (auto* claimMarker : claimMarkers) {
        // TODO: Send password?
    }
}
