// Entity::Initialize() {
	/**
	 * Setup trigger
	 */

	// const auto triggerInfo = GetVarAsString(u"trigger_id");

	// if (!triggerInfo.empty()) m_Components.emplace(eReplicaComponentType::TRIGGER, new TriggerComponent(this, triggerInfo));

	/**
	 * Setup groups
	 */

	// const auto groupIDs = GetVarAsString(u"groupID");

	// if (!groupIDs.empty()) {
	// 	m_Groups = GeneralUtils::SplitString(groupIDs, ';');
	// 	if (m_Groups.back().empty()) m_Groups.erase(m_Groups.end() - 1);
	// }

	/**
	 * Set ourselves as a child of our parent
	 */

	// if (m_ParentEntity != nullptr) {
	// 	m_ParentEntity->AddChild(this);
	// }

	// Get the registry table
	// CDComponentsRegistryTable* compRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();

	/**
	 * Special case for BBB models. They have components not corresponding to the registry.
	 */

	// if (m_TemplateID == 14) {
		// const auto simplePhysicsComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SIMPLE_PHYSICS);

		// SimplePhysicsComponent* comp = new SimplePhysicsComponent(simplePhysicsComponentID, this);
		// m_Components.insert(std::make_pair(eReplicaComponentType::SIMPLE_PHYSICS, comp));

		// ModelComponent* modelcomp = new ModelComponent(this);
		// m_Components.insert(std::make_pair(eReplicaComponentType::MODEL, modelcomp));

		// RenderComponent* render = new RenderComponent(this);
		// m_Components.insert(std::make_pair(eReplicaComponentType::RENDER, render));

		// auto destroyableComponent = new DestroyableComponent(this);
		destroyableComponent->SetHealth(1);
		destroyableComponent->SetMaxHealth(1.0f);
		destroyableComponent->SetFaction(-1, true);
		destroyableComponent->SetIsSmashable(true);
		// m_Components.insert(std::make_pair(eReplicaComponentType::DESTROYABLE, destroyableComponent));
		// We have all our components.
		// return;
	// }

	/**
	 * Go through all the components and check if this entity has them.
	 *
	 * Not all components are implemented. Some are represented by a nullptr, as they hold no data.
	 */

	// if (GetParentUser()) {
	// 	auto missions = new MissionComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::MISSION, missions));
	// 	missions->LoadFromXml(m_Character->GetXMLDoc());
	// }

	// uint32_t petComponentId = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PET);
	// if (petComponentId > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::PET, new PetComponent(this, petComponentId)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::ZONE_CONTROL) > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::ZONE_CONTROL, nullptr));
	// }

	// uint32_t possessableComponentId = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::POSSESSABLE);
	// if (possessableComponentId > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::POSSESSABLE, new PossessableComponent(this, possessableComponentId)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::MODULE_ASSEMBLY) > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::MODULE_ASSEMBLY, new ModuleAssemblyComponent(this)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::RACING_STATS) > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::RACING_STATS, nullptr));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::LUP_EXHIBIT, -1) >= 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::LUP_EXHIBIT, new LUPExhibitComponent(this)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::RACING_CONTROL) > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::RACING_CONTROL, new RacingControlComponent(this)));
	// }

	// const auto propertyEntranceComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PROPERTY_ENTRANCE);
	// if (propertyEntranceComponentID > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::PROPERTY_ENTRANCE,
	// 		new PropertyEntranceComponent(propertyEntranceComponentID, this)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::CONTROLLABLE_PHYSICS) > 0) {
	// 	ControllablePhysicsComponent* controllablePhysics = new ControllablePhysicsComponent(this);

	// 	if (m_Character) {
	// 		controllablePhysics->LoadFromXml(m_Character->GetXMLDoc());

	// 		const auto mapID = Game::server->GetZoneID();

	// 		//If we came from another zone, put us in the starting loc
	// 		if (m_Character->GetZoneID() != Game::server->GetZoneID() || mapID == 1603) { // Exception for Moon Base as you tend to spawn on the roof.
	// 			NiPoint3 pos;
	// 			NiQuaternion rot;

	// 			const auto& targetSceneName = m_Character->GetTargetScene();
	// 			auto* targetScene = EntityManager::Instance()->GetSpawnPointEntity(targetSceneName);

	// 			if (m_Character->HasBeenToWorld(mapID) && targetSceneName.empty()) {
	// 				pos = m_Character->GetRespawnPoint(mapID);
	// 				rot = dZoneManager::Instance()->GetZone()->GetSpawnRot();
	// 			} else if (targetScene != nullptr) {
	// 				pos = targetScene->GetPosition();
	// 				rot = targetScene->GetRotation();
	// 			} else {
	// 				pos = dZoneManager::Instance()->GetZone()->GetSpawnPos();
	// 				rot = dZoneManager::Instance()->GetZone()->GetSpawnRot();
	// 			}

	// 			controllablePhysics->SetPosition(pos);
	// 			controllablePhysics->SetRotation(rot);
	// 		}
	// 	} else {
	// 		controllablePhysics->SetPosition(m_DefaultPosition);
	// 		controllablePhysics->SetRotation(m_DefaultRotation);
	// 	}

	// 	m_Components.insert(std::make_pair(eReplicaComponentType::CONTROLLABLE_PHYSICS, controllablePhysics));
	// }

	// If an entity is marked a phantom, simple physics is made into phantom phyics.
	// bool markedAsPhantom = GetVar<bool>(u"markedAsPhantom");

	// const auto simplePhysicsComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SIMPLE_PHYSICS);
	// if (!markedAsPhantom && simplePhysicsComponentID > 0) {
	// 	SimplePhysicsComponent* comp = new SimplePhysicsComponent(simplePhysicsComponentID, this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::SIMPLE_PHYSICS, comp));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS) > 0) {
	// 	RigidbodyPhantomPhysicsComponent* comp = new RigidbodyPhantomPhysicsComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::RIGID_BODY_PHANTOM_PHYSICS, comp));
	// }

	// if (markedAsPhantom || compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PHANTOM_PHYSICS) > 0) {
	// 	PhantomPhysicsComponent* phantomPhysics = new PhantomPhysicsComponent(this);
		phantomPhysics->SetPhysicsEffectActive(false);
		// m_Components.insert(std::make_pair(eReplicaComponentType::PHANTOM_PHYSICS, phantomPhysics));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::VEHICLE_PHYSICS) > 0) {
		// VehiclePhysicsComponent* vehiclePhysicsComponent = new VehiclePhysicsComponent(this);
		// m_Components.insert(std::make_pair(eReplicaComponentType::VEHICLE_PHYSICS, vehiclePhysicsComponent));
		vehiclePhysicsComponent->SetPosition(m_DefaultPosition);
		vehiclePhysicsComponent->SetRotation(m_DefaultRotation);
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SOUND_TRIGGER, -1) != -1) {
	// 	auto* comp = new SoundTriggerComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::SOUND_TRIGGER, comp));
	// }

	//Also check for the collectible id:
	// m_CollectibleID = GetVarAs<int32_t>(u"collectible_id");

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::BUFF) > 0) {
	// 	BuffComponent* comp = new BuffComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::BUFF, comp));
	// }

	// int collectibleComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::COLLECTIBLE);

	// if (collectibleComponentID > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::COLLECTIBLE, nullptr));
	// }

	/**
	 * Multiple components require the destructible component.
	 */

	// int destroyableComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::DESTROYABLE);
	// int quickBuildComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::QUICK_BUILD);

	// int componentID = 0;
	// if (collectibleComponentID > 0) componentID = collectibleComponentID;
	// if (quickBuildComponentID > 0) componentID = quickBuildComponentID;
	// if (destroyableComponentID > 0) componentID = destroyableComponentID;

	// CDDestructibleComponentTable* destCompTable = CDClientManager::Instance().GetTable<CDDestructibleComponentTable>();
	// std::vector<CDDestructibleComponent> destCompData = destCompTable->Query([=](CDDestructibleComponent entry) { return (entry.id == componentID); });

	// if (destroyableComponentID > 0 || collectibleComponentID > 0) {
		// DestroyableComponent* comp = new DestroyableComponent(this);
		if (m_Character) {
			comp->LoadFromXml(m_Character->GetXMLDoc());
		} else {
			if (componentID > 0) {
				// std::vector<CDDestructibleComponent> destCompData = destCompTable->Query([=](CDDestructibleComponent entry) { return (entry.id == componentID); });

				if (destCompData.size() > 0) {
					if (HasComponent(eReplicaComponentType::RACING_STATS)) {
						destCompData[0].imagination = 60;
					}

					comp->SetHealth(destCompData[0].life);
					comp->SetImagination(destCompData[0].imagination);
					comp->SetArmor(destCompData[0].armor);

					comp->SetMaxHealth(destCompData[0].life);
					comp->SetMaxImagination(destCompData[0].imagination);
					comp->SetMaxArmor(destCompData[0].armor);

					comp->SetIsSmashable(destCompData[0].isSmashable);

					comp->SetLootMatrixID(destCompData[0].LootMatrixIndex);

					// Now get currency information
					uint32_t npcMinLevel = destCompData[0].level;
					uint32_t currencyIndex = destCompData[0].CurrencyIndex;

					CDCurrencyTableTable* currencyTable = CDClientManager::Instance().GetTable<CDCurrencyTableTable>();
					std::vector<CDCurrencyTable> currencyValues = currencyTable->Query([=](CDCurrencyTable entry) { return (entry.currencyIndex == currencyIndex && entry.npcminlevel == npcMinLevel); });

					if (currencyValues.size() > 0) {
						// Set the coins
						comp->SetMinCoins(currencyValues[0].minvalue);
						comp->SetMaxCoins(currencyValues[0].maxvalue);
					}

					// extraInfo overrides
					comp->SetIsSmashable(GetVarAs<int32_t>(u"is_smashable") != 0);
				}
			} else {
				comp->SetHealth(1);
				comp->SetArmor(0);

				comp->SetMaxHealth(1);
				comp->SetMaxArmor(0);

				comp->SetIsSmashable(true);
				comp->AddFaction(-1);
				comp->AddFaction(6); //Smashables

				// A race car has 60 imagination, other entities defaults to 0.
				comp->SetImagination(HasComponent(eReplicaComponentType::RACING_STATS) ? 60 : 0);
				comp->SetMaxImagination(HasComponent(eReplicaComponentType::RACING_STATS) ? 60 : 0);
			}
		}

		if (destCompData.size() > 0) {
			comp->AddFaction(destCompData[0].faction);
			std::stringstream ss(destCompData[0].factionList);
			std::string token;

			while (std::getline(ss, token, ',')) {
				if (std::stoi(token) == destCompData[0].faction) continue;

				if (token != "") {
					comp->AddFaction(std::stoi(token));
				}
			}
		}

		// m_Components.insert(std::make_pair(eReplicaComponentType::DESTROYABLE, comp));
	}

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::CHARACTER) > 0 || m_Character) {
		// Character Component always has a possessor, level, and forced movement components
		// m_Components.insert(std::make_pair(eReplicaComponentType::POSSESSOR, new PossessorComponent(this)));

		// load in the xml for the level
		// auto* levelComp = new LevelProgressionComponent(this);
		// levelComp->LoadFromXml(m_Character->GetXMLDoc());
		// m_Components.insert(std::make_pair(eReplicaComponentType::LEVEL_PROGRESSION, levelComp));

		// m_Components.insert(std::make_pair(eReplicaComponentType::PLAYER_FORCED_MOVEMENT, new PlayerForcedMovementComponent(this)));

		// CharacterComponent* charComp = new CharacterComponent(this, m_Character);
		// charComp->LoadFromXml(m_Character->GetXMLDoc());
		// m_Components.insert(std::make_pair(eReplicaComponentType::CHARACTER, charComp));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::INVENTORY) > 0 || m_Character) {
	// 	InventoryComponent* comp = nullptr;
	// 	if (m_Character) comp = new InventoryComponent(this, m_Character->GetXMLDoc());
	// 	else comp = new InventoryComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::INVENTORY, comp));
	// }
	// if this component exists, then we initialize it. it's value is always 0
	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::ROCKET_LAUNCH_LUP, -1) != -1) {
	// 	auto comp = new RocketLaunchLupComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::ROCKET_LAUNCH_LUP, comp));
	// }

	/**
	 * This is a bit of a mess
	 */

	CDScriptComponentTable* scriptCompTable = CDClientManager::Instance().GetTable<CDScriptComponentTable>();
	int32_t scriptComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SCRIPT, -1);

	std::string scriptName = "";
	bool client = false;
	if (scriptComponentID > 0 || m_Character) {
		std::string clientScriptName;
		if (!m_Character) {
			CDScriptComponent scriptCompData = scriptCompTable->GetByID(scriptComponentID);
			scriptName = scriptCompData.script_name;
			clientScriptName = scriptCompData.client_script_name;
		} else {
			scriptName = "";
		}

		if (scriptName != "" || (scriptName == "" && m_Character)) {

		} else if (clientScriptName != "") {
			client = true;
		} else if (!m_Character) {
			client = true;
		}
	}

	std::string customScriptServer;
	bool hasCustomServerScript = false;

	const auto customScriptServerName = GetVarAsString(u"custom_script_server");
	const auto customScriptClientName = GetVarAsString(u"custom_script_client");

	if (!customScriptServerName.empty()) {
		customScriptServer = customScriptServerName;
		hasCustomServerScript = true;
	}

	if (!customScriptClientName.empty()) {
		client = true;
	}

	if (hasCustomServerScript && scriptName.empty()) {
		scriptName = customScriptServer;
	}

	if (!scriptName.empty() || client || m_Character || scriptComponentID >= 0) {
		m_Components.insert(std::make_pair(eReplicaComponentType::SCRIPT, new ScriptComponent(this, scriptName, true, client && scriptName.empty())));
	}

	// ZoneControl script
	if (m_TemplateID == 2365) {
		CDZoneTableTable* zoneTable = CDClientManager::Instance().GetTable<CDZoneTableTable>();
		const auto zoneID = dZoneManager::Instance()->GetZoneID();
		const CDZoneTable* zoneData = zoneTable->Query(zoneID.GetMapID());

		if (zoneData != nullptr) {
			int zoneScriptID = zoneData->scriptID;
			CDScriptComponent zoneScriptData = scriptCompTable->GetByID(zoneScriptID);

			ScriptComponent* comp = new ScriptComponent(this, zoneScriptData.script_name, true);
			m_Components.insert(std::make_pair(eReplicaComponentType::SCRIPT, comp));
		}
	}

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SKILL, -1) != -1 || m_Character) {
	// 	SkillComponent* comp = new SkillComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::SKILL, comp));
	// }

	// const auto combatAiId = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::BASE_COMBAT_AI);
	// if (combatAiId > 0) {
	// 	BaseCombatAIComponent* comp = new BaseCombatAIComponent(this, combatAiId);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::BASE_COMBAT_AI, comp));
	// }

	if (int componentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::QUICK_BUILD) > 0) {
		QuickBuildComponent* comp = new QuickBuildComponent(this);
		m_Components.insert(std::make_pair(eReplicaComponentType::QUICK_BUILD, comp));

		CDRebuildComponentTable* rebCompTable = CDClientManager::Instance().GetTable<CDRebuildComponentTable>();
		std::vector<CDRebuildComponent> rebCompData = rebCompTable->Query([=](CDRebuildComponent entry) { return (entry.id == quickBuildComponentID); });

		if (rebCompData.size() > 0) {
			comp->SetResetTime(rebCompData[0].reset_time);
			comp->SetCompleteTime(rebCompData[0].complete_time);
			comp->SetTakeImagination(rebCompData[0].take_imagination);
			comp->SetInterruptible(rebCompData[0].interruptible);
			comp->SetSelfActivator(rebCompData[0].self_activator);
			comp->SetActivityId(rebCompData[0].activityID);
			comp->SetPostImaginationCost(rebCompData[0].post_imagination_cost);
			comp->SetTimeBeforeSmash(rebCompData[0].time_before_smash);

			const auto rebuildResetTime = GetVar<float>(u"rebuild_reset_time");

			if (rebuildResetTime != 0.0f) {
				comp->SetResetTime(rebuildResetTime);

				if (m_TemplateID == 9483) // Look away!
				{
					comp->SetResetTime(comp->GetResetTime() + 25);
				}
			}

			const auto activityID = GetVar<int32_t>(u"activityID");

			if (activityID > 0) {
				comp->SetActivityId(activityID);
			}

			const auto compTime = GetVar<float>(u"compTime");

			if (compTime > 0) {
				comp->SetCompleteTime(compTime);
			}
		}
	}

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SWITCH, -1) != -1) {
	// 	SwitchComponent* comp = new SwitchComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::SWITCH, comp));
	// }

	// if ((compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::VENDOR) > 0)) {
	// 	VendorComponent* comp = new VendorComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::VENDOR, comp));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PROPERTY_VENDOR, -1) != -1) {
	// 	auto* component = new PropertyVendorComponent(this);
	// 	m_Components.insert_or_assign(eReplicaComponentType::PROPERTY_VENDOR, component);
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PROPERTY_MANAGEMENT, -1) != -1) {
	// 	auto* component = new PropertyManagementComponent(this);
	// 	m_Components.insert_or_assign(eReplicaComponentType::PROPERTY_MANAGEMENT, component);
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::BOUNCER, -1) != -1) { // you have to determine it like this because all bouncers have a componentID of 0
	// 	BouncerComponent* comp = new BouncerComponent(this);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::BOUNCER, comp));
	// }

	// int32_t renderComponentId = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::RENDER);
	// if ((renderComponentId > 0 && m_TemplateID != 2365) || m_Character) {
	// 	RenderComponent* render = new RenderComponent(this, renderComponentId);
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::RENDER, render));
	// }

	// if ((compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::MISSION_OFFER) > 0) || m_Character) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::MISSION_OFFER, new MissionOfferComponent(this, m_TemplateID)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::BUILD_BORDER, -1) != -1) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::BUILD_BORDER, new BuildBorderComponent(this)));
	// }

	// Scripted activity component
	// int scriptedActivityID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SCRIPTED_ACTIVITY);
	// if ((scriptedActivityID > 0)) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::SCRIPTED_ACTIVITY, new ScriptedActivityComponent(this, scriptedActivityID)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::MODEL, -1) != -1 && !GetComponent<PetComponent>()) {
		// m_Components.insert(std::make_pair(eReplicaComponentType::MODEL, new ModelComponent(this)));
		if (m_Components.find(eReplicaComponentType::DESTROYABLE) == m_Components.end()) {
			auto destroyableComponent = new DestroyableComponent(this);
			destroyableComponent->SetHealth(1);
			destroyableComponent->SetMaxHealth(1.0f);
			destroyableComponent->SetFaction(-1, true);
			destroyableComponent->SetIsSmashable(true);
			m_Components.insert(std::make_pair(eReplicaComponentType::DESTROYABLE, destroyableComponent));
		}
	// }

	// PetComponent* petComponent;
	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::ITEM) > 0 && !TryGetComponent(eReplicaComponentType::PET, petComponent) && !HasComponent(eReplicaComponentType::MODEL)) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::ITEM, nullptr));
	// }

	// Shooting gallery component
	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::SHOOTING_GALLERY) > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::SHOOTING_GALLERY, new ShootingGalleryComponent(this)));
	// }

	// if (compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PROPERTY, -1) != -1) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::PROPERTY, new PropertyComponent(this)));
	// }

	// const int rocketId = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::ROCKET_LAUNCH);
	// if ((rocketId > 0)) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::ROCKET_LAUNCH, new RocketLaunchpadControlComponent(this, rocketId)));
	// }

	// const int32_t railComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::RAIL_ACTIVATOR);
	// if (railComponentID > 0) {
	// 	m_Components.insert(std::make_pair(eReplicaComponentType::RAIL_ACTIVATOR, new RailActivatorComponent(this, railComponentID)));
	// }

	int movementAIID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::MOVEMENT_AI);
	if (movementAIID > 0) {
		CDMovementAIComponentTable* moveAITable = CDClientManager::Instance().GetTable<CDMovementAIComponentTable>();
		std::vector<CDMovementAIComponent> moveAIComp = moveAITable->Query([=](CDMovementAIComponent entry) {return (entry.id == movementAIID); });

		if (moveAIComp.size() > 0) {
			MovementAIInfo moveInfo = MovementAIInfo();

			moveInfo.movementType = moveAIComp[0].MovementType;
			moveInfo.wanderChance = moveAIComp[0].WanderChance;
			moveInfo.wanderRadius = moveAIComp[0].WanderRadius;
			moveInfo.wanderSpeed = moveAIComp[0].WanderSpeed;
			moveInfo.wanderDelayMax = moveAIComp[0].WanderDelayMax;
			moveInfo.wanderDelayMin = moveAIComp[0].WanderDelayMin;

			bool useWanderDB = GetVar<bool>(u"usewanderdb");

			if (!useWanderDB) {
				const auto wanderOverride = GetVarAs<float>(u"wanderRadius");

				if (wanderOverride != 0.0f) {
					moveInfo.wanderRadius = wanderOverride;
				}
			}

			m_Components.insert(std::make_pair(eReplicaComponentType::MOVEMENT_AI, new MovementAIComponent(this, moveInfo)));
		}
	} else if (petComponentId > 0 || combatAiId > 0 && GetComponent<BaseCombatAIComponent>()->GetTetherSpeed() > 0) {
		MovementAIInfo moveInfo = MovementAIInfo();
		moveInfo.movementType = "";
		moveInfo.wanderChance = 0;
		moveInfo.wanderRadius = 16;
		moveInfo.wanderSpeed = 2.5f;
		moveInfo.wanderDelayMax = 5;
		moveInfo.wanderDelayMin = 2;

		m_Components.insert(std::make_pair(eReplicaComponentType::MOVEMENT_AI, new MovementAIComponent(this, moveInfo)));
	}

	std::string pathName = GetVarAsString(u"attached_path");
	const Path* path = dZoneManager::Instance()->GetZone()->GetPath(pathName);

	//Check to see if we have an attached path and add the appropiate component to handle it:
	if (path){
		// if we have a moving platform path, then we need a moving platform component
		if (path->pathType == PathType::MovingPlatform) {
			MovingPlatformComponent* plat = new MovingPlatformComponent(this, pathName);
			m_Components.insert(std::make_pair(eReplicaComponentType::MOVING_PLATFORM, plat));
		// else if we are a movement path
		} /*else if (path->pathType == PathType::Movement) {
			auto movementAIcomp = GetComponent<MovementAIComponent>();
			if (movementAIcomp){
				// TODO: set path in existing movementAIComp
			} else {
				// TODO: create movementAIcomp and set path
			}
		}*/
	} 
	// else {
		// else we still need to setup moving platform if it has a moving platform comp but no path
		// int32_t movingPlatformComponentId = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::MOVING_PLATFORM, -1);
		// if (movingPlatformComponentId >= 0) {
		// 	MovingPlatformComponent* plat = new MovingPlatformComponent(this, pathName);
		// 	m_Components.insert(std::make_pair(eReplicaComponentType::MOVING_PLATFORM, plat));
		// }
	// }

	int proximityMonitorID = compRegistryTable->GetByIDAndType(m_TemplateID, eReplicaComponentType::PROXIMITY_MONITOR);
	if (proximityMonitorID > 0) {
		CDProximityMonitorComponentTable* proxCompTable = CDClientManager::Instance().GetTable<CDProximityMonitorComponentTable>();
		std::vector<CDProximityMonitorComponent> proxCompData = proxCompTable->Query([=](CDProximityMonitorComponent entry) { return (entry.id == proximityMonitorID); });
		if (proxCompData.size() > 0) {
			std::vector<std::string> proximityStr = GeneralUtils::SplitString(proxCompData[0].Proximities, ',');
			ProximityMonitorComponent* comp = new ProximityMonitorComponent(this, std::stoi(proximityStr[0]), std::stoi(proximityStr[1]));
			m_Components.insert(std::make_pair(eReplicaComponentType::PROXIMITY_MONITOR, comp));
		}
	}

	// Hacky way to trigger these when the object has had a chance to get constructed
	AddCallbackTimer(0, [this]() {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
			script->OnStartup(this);
		}
		});

	if (!m_Character && EntityManager::Instance()->GetGhostingEnabled()) {
		// Don't ghost what is likely large scene elements
		if (HasComponent(eReplicaComponentType::SIMPLE_PHYSICS) && HasComponent(eReplicaComponentType::RENDER) && (m_Components.size() == 2 || (HasComponent(eReplicaComponentType::TRIGGER) && m_Components.size() == 3))) {
			goto no_ghosting;
		}

		/* Filter for ghosting candidates.
		 *
		 * Don't ghost moving platforms, until we've got proper syncing for those.
		 * Don't ghost big phantom physics triggers, as putting those to sleep might prevent interactions.
		 * Don't ghost property related objects, as the client expects those to always be loaded.
		 */
		if (
			!EntityManager::IsExcludedFromGhosting(GetLOT()) &&
			!HasComponent(eReplicaComponentType::SCRIPTED_ACTIVITY) &&
			!HasComponent(eReplicaComponentType::MOVING_PLATFORM) &&
			!HasComponent(eReplicaComponentType::PHANTOM_PHYSICS) &&
			!HasComponent(eReplicaComponentType::PROPERTY) &&
			!HasComponent(eReplicaComponentType::RACING_CONTROL) &&
			!HasComponent(eReplicaComponentType::VEHICLE_PHYSICS)
			)
			//if (HasComponent(eReplicaComponentType::BASE_COMBAT_AI))
		{
			m_IsGhostingCandidate = true;
		}

		if (GetLOT() == 6368) {
			m_IsGhostingCandidate = true;
		}

		// Special case for collectibles in Ninjago
		if (HasComponent(eReplicaComponentType::COLLECTIBLE) && Game::server->GetZoneID() == 2000) {
			m_IsGhostingCandidate = true;
		}
	}

no_ghosting:

	TriggerEvent(eTriggerEventType::CREATE, this);

	if (m_Character) {
		auto controllablePhysicsComponent = GetComponent<ControllablePhysicsComponent>();
		auto levelComponent = GetComponent<LevelProgressionComponent>();

		if (controllablePhysicsComponent && levelComponent) {
			controllablePhysicsComponent->SetSpeedMultiplier(levelComponent->GetSpeedBase() / 500.0f);
		}
	}
// }
