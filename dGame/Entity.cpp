#include "dCommonVars.h"
#include "Entity.h"
#include "CDClientManager.h"
#include "Game.h"
#include "dLogger.h"
#include <PacketUtils.h>
#include <functional>
#include "CDDestructibleComponentTable.h"
#include "CDClientDatabase.h"
#include <sstream>
#include "dServer.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "dZoneManager.h"
#include "Zone.h"
#include "Spawner.h"
#include "UserManager.h"
#include "dpWorld.h"
#include "Player.h"

//Component includes:
#include "Component.h"
#include "ControllablePhysicsComponent.h"
#include "RenderComponent.h"
#include "RocketLaunchLupComponent.h"
#include "CharacterComponent.h"
#include "DestroyableComponent.h"
#include "BuffComponent.h"
#include "BouncerComponent.h"
#include "InventoryComponent.h"
#include "ScriptComponent.h"
#include "SkillComponent.h"
#include "SimplePhysicsComponent.h"
#include "SwitchComponent.h"
#include "PhantomPhysicsComponent.h"
#include "RigidbodyPhantomPhysicsComponent.h"
#include "MovingPlatformComponent.h"
#include "MissionComponent.h"
#include "MissionOfferComponent.h"
#include "RebuildComponent.h"
#include "BuildBorderComponent.h"
#include "MovementAIComponent.h"
#include "VendorComponent.h"
#include "RocketLaunchpadControlComponent.h"
#include "PropertyComponent.h"
#include "BaseCombatAIComponent.h"
#include "PropertyManagementComponent.h"
#include "PropertyVendorComponent.h"
#include "ProximityMonitorComponent.h"
#include "PropertyEntranceComponent.h"
#include "ModelComponent.h"
#include "ZCompression.h"
#include "PetComponent.h"
#include "VehiclePhysicsComponent.h"
#include "PossessableComponent.h"
#include "PossessorComponent.h"
#include "ModuleAssemblyComponent.h"
#include "RacingControlComponent.h"
#include "SoundTriggerComponent.h"
#include "ShootingGalleryComponent.h"
#include "RailActivatorComponent.h"
#include "LUPExhibitComponent.h"

Entity::Entity(const LWOOBJID& objectID, EntityInfo info, Entity* parentEntity) {
	m_ObjectID = objectID;
	m_TemplateID = info.lot;
	m_ParentEntity = parentEntity;
	m_Character = nullptr;
	m_GMLevel = 0;
	m_CollectibleID = 0;
	m_Trigger = nullptr; //new LUTriggers::Trigger();
	m_NetworkID = 0;
	m_Groups = {};
	m_OwnerOverride = LWOOBJID_EMPTY;
	m_Timers = {};
	m_ChildEntities = {};
	m_ScheduleKiller = nullptr;
	m_TargetsInPhantom = {};
	m_Components = {};
	m_DieCallbacks = {};
	m_PhantomCollisionCallbacks = {};
	m_IsParentChildDirty = true;

	m_Settings = info.settings;
	m_NetworkSettings = info.networkSettings;
	m_DefaultPosition = info.pos;
	m_DefaultRotation = info.rot;
	m_Scale = info.scale;
	m_Spawner = info.spawner;
	m_SpawnerID = info.spawnerID;
    m_HasSpawnerNodeID = info.hasSpawnerNodeID;
    m_SpawnerNodeID = info.spawnerNodeID;

	if (info.lot != 1) m_PlayerIsReadyForUpdates = true;
}

Entity::~Entity() {
	if (m_Character) {
		m_Character->SaveXMLToDatabase();
	}

	CancelAllTimers();
	CancelCallbackTimers();

	const auto components = m_Components;
	
	for (const auto& pair : components) {
		delete pair.second;

		m_Components.erase(pair.first);
	}

	for (auto child : m_ChildEntities) {
		if (child) child->RemoveParent();
	}

	if (m_ParentEntity) {
		m_ParentEntity->RemoveChild(this);
	}
}

void Entity::Initialize()
{
	/**
	 * Setup trigger
	 */

	const auto triggerName = GetVarAsString(u"trigger_id");

	if (!triggerName.empty()) {
		std::stringstream ss(triggerName);
		std::vector<std::string> tokens;
		std::string token;
		while (std::getline(ss, token, ':')) {
			tokens.push_back(token);
		}

		uint32_t sceneID = std::stoi(tokens[0]);
		uint32_t triggerID = std::stoi(tokens[1]);

		if (m_Trigger != nullptr) {
			delete m_Trigger;
			m_Trigger = nullptr;
		}

		m_Trigger = dZoneManager::Instance()->GetZone()->GetTrigger(sceneID, triggerID);

		if (m_Trigger == nullptr) {
			m_Trigger = new LUTriggers::Trigger();
		}
	}

	/**
	 * Setup groups
	 */

	const auto groupIDs = GetVarAsString(u"groupID");

	if (!groupIDs.empty()) {
		m_Groups = GeneralUtils::SplitString(groupIDs, ';');
		m_Groups.erase(m_Groups.end() - 1);
	}

	/**
	 * Set ourselves as a child of our parent
	 */

	if (m_ParentEntity != nullptr) {
		m_ParentEntity->AddChild(this);
	}

	// Get the registry table
	CDComponentsRegistryTable* compRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");

	/**
	 * Special case for BBB models. They have components not corresponding to the registry.
	 */

	if (m_TemplateID == 14) {
		const auto simplePhysicsComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SIMPLE_PHYSICS);

		SimplePhysicsComponent* comp = new SimplePhysicsComponent(simplePhysicsComponentID, this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_SIMPLE_PHYSICS, comp));

		ModelComponent* modelcomp = new ModelComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_MODEL, modelcomp));

		RenderComponent* render = new RenderComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_RENDER, render));

		auto destroyableComponent = new DestroyableComponent(this);
		destroyableComponent->SetHealth(1);
		destroyableComponent->SetMaxHealth(1.0f);
		destroyableComponent->SetFaction(-1, true);
		destroyableComponent->SetIsSmashable(true);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_DESTROYABLE, destroyableComponent));
		// We have all our components.
		return; 
	}

	/**
	 * Go through all the components and check if this entity has them.
	 * 
	 * Not all components are implemented. Some are represented by a nullptr, as they hold no data.
	 */

	if (GetParentUser()) {
		auto missions = new MissionComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_MISSION, missions));
		missions->LoadFromXml(m_Character->GetXMLDoc());
	}

	uint32_t petComponentId = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PET);
	if (petComponentId > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_PET, new PetComponent(this, petComponentId)));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_ZONE_CONTROL) > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_ZONE_CONTROL, nullptr));
	}

	uint32_t possessableComponentId = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_POSSESSABLE);
	if (possessableComponentId > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_POSSESSABLE, new PossessableComponent(this, possessableComponentId)));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_MODULE_ASSEMBLY) > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_MODULE_ASSEMBLY, new ModuleAssemblyComponent(this)));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_RACING_STATS) > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_RACING_STATS, nullptr));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_EXHIBIT, -1) >= 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_EXHIBIT, new LUPExhibitComponent(this)));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_RACING_CONTROL) > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_RACING_CONTROL, new RacingControlComponent(this)));
	}

	const auto propertyEntranceComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PROPERTY_ENTRANCE);
	if (propertyEntranceComponentID > 0) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_PROPERTY_ENTRANCE,
									 new PropertyEntranceComponent(propertyEntranceComponentID, this)));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_CONTROLLABLE_PHYSICS) > 0) {
		ControllablePhysicsComponent* controllablePhysics = new ControllablePhysicsComponent(this);

		if (m_Character) {
			controllablePhysics->LoadFromXML(m_Character->GetXMLDoc());

			const auto mapID = Game::server->GetZoneID();

			//If we came from another zone, put us in the starting loc
			if (m_Character->GetZoneID() != Game::server->GetZoneID() || mapID == 1603) { // Exception for Moon Base as you tend to spawn on the roof.
				NiPoint3 pos;
				NiQuaternion rot;

				const auto& targetSceneName = m_Character->GetTargetScene();
				auto* targetScene = EntityManager::Instance()->GetSpawnPointEntity(targetSceneName);

				if (m_Character->HasBeenToWorld(mapID) && targetSceneName.empty()) {
					pos = m_Character->GetRespawnPoint(mapID);
					rot = dZoneManager::Instance()->GetZone()->GetSpawnRot();
				}
				else if (targetScene != nullptr) {
					pos = targetScene->GetPosition();
					rot = targetScene->GetRotation();
				}
				else {
				 	pos = dZoneManager::Instance()->GetZone()->GetSpawnPos();
				 	rot = dZoneManager::Instance()->GetZone()->GetSpawnRot();
				}

				controllablePhysics->SetPosition(pos);
				controllablePhysics->SetRotation(rot);
			}
		}
		else {
			controllablePhysics->SetPosition(m_DefaultPosition);
			controllablePhysics->SetRotation(m_DefaultRotation);
		}

		m_Components.insert(std::make_pair(COMPONENT_TYPE_CONTROLLABLE_PHYSICS, controllablePhysics));
	}

	// If an entity is marked a phantom, simple physics is made into phantom phyics.
	bool markedAsPhantom = GetVar<bool>(u"markedAsPhantom");

	const auto simplePhysicsComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SIMPLE_PHYSICS);
	if (!markedAsPhantom && simplePhysicsComponentID > 0) {
		SimplePhysicsComponent* comp = new SimplePhysicsComponent(simplePhysicsComponentID, this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_SIMPLE_PHYSICS, comp));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_RIGID_BODY_PHANTOM_PHYSICS) > 0) {
		RigidbodyPhantomPhysicsComponent* comp = new RigidbodyPhantomPhysicsComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_RIGID_BODY_PHANTOM_PHYSICS, comp));
	}

	if (markedAsPhantom || compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PHANTOM_PHYSICS) > 0) {
		PhantomPhysicsComponent* phantomPhysics = new PhantomPhysicsComponent(this);
		phantomPhysics->SetPhysicsEffectActive(false);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_PHANTOM_PHYSICS, phantomPhysics));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_VEHICLE_PHYSICS) > 0) {
		VehiclePhysicsComponent* vehiclePhysicsComponent = new VehiclePhysicsComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_VEHICLE_PHYSICS, vehiclePhysicsComponent));
		vehiclePhysicsComponent->SetPosition(m_DefaultPosition);
		vehiclePhysicsComponent->SetRotation(m_DefaultRotation);
	}
	
	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SOUND_TRIGGER, -1) != -1) {
	    auto* comp = new SoundTriggerComponent(this);
	    m_Components.insert(std::make_pair(COMPONENT_TYPE_SOUND_TRIGGER, comp));
	}

	//Check to see if we have a moving platform component:
	//Which, for some reason didn't get added to the ComponentsRegistry so we have to check for a path manually here.
	std::string attachedPath = GetVarAsString(u"attached_path");

	if (!attachedPath.empty() || compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_MOVING_PLATFORM, -1) != -1) {
		MovingPlatformComponent* plat = new MovingPlatformComponent(this, attachedPath);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_MOVING_PLATFORM, plat));
	}

	//Also check for the collectible id:
	m_CollectibleID = GetVarAs<int32_t>(u"collectible_id");

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_BUFF) > 0) {
		BuffComponent* comp = new BuffComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_BUFF, comp));
	}

	int collectibleComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_COLLECTIBLE);

	if (collectibleComponentID > 0){
		m_Components.insert(std::make_pair(COMPONENT_TYPE_COLLECTIBLE, nullptr));
	}

	/**
	 * Multiple components require the destructible component.
	 */

	int buffComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_BUFF);
	int rebuildComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_REBUILD);

	int componentID = 0;
	if (collectibleComponentID > 0) componentID = collectibleComponentID;
	if (rebuildComponentID > 0) componentID = rebuildComponentID;
	if (buffComponentID > 0) componentID = buffComponentID;

	CDDestructibleComponentTable* destCompTable = CDClientManager::Instance()->GetTable<CDDestructibleComponentTable>("DestructibleComponent");
	std::vector<CDDestructibleComponent> destCompData = destCompTable->Query([=](CDDestructibleComponent entry) { return (entry.id == componentID); });

	if (buffComponentID > 0 || collectibleComponentID > 0) {
		DestroyableComponent* comp = new DestroyableComponent(this);
		if (m_Character) {
			comp->LoadFromXML(m_Character->GetXMLDoc());
		}
		else {
			if (componentID > 0) {
				std::vector<CDDestructibleComponent> destCompData = destCompTable->Query([=](CDDestructibleComponent entry) { return (entry.id == componentID); });

				if (destCompData.size() > 0) {
					if (HasComponent(COMPONENT_TYPE_RACING_STATS))
					{
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

					CDCurrencyTableTable* currencyTable = CDClientManager::Instance()->GetTable<CDCurrencyTableTable>("CurrencyTable");
					std::vector<CDCurrencyTable> currencyValues = currencyTable->Query([=](CDCurrencyTable entry) { return (entry.currencyIndex == currencyIndex && entry.npcminlevel == npcMinLevel); });

					if (currencyValues.size() > 0) {
						// Set the coins
						comp->SetMinCoins(currencyValues[0].minvalue);
						comp->SetMaxCoins(currencyValues[0].maxvalue);
					}

					// extraInfo overrides
					comp->SetIsSmashable(GetVarAs<int32_t>(u"is_smashable") != 0);
				}
			}
			else {
				comp->SetHealth(1);
				comp->SetArmor(0);

				comp->SetMaxHealth(1);
				comp->SetMaxArmor(0);

				comp->SetIsSmashable(true);
				comp->AddFaction(-1);
				comp->AddFaction(6); //Smashables

				// A race car has 60 imagination, other entities defaults to 0.
				comp->SetImagination(HasComponent(COMPONENT_TYPE_RACING_STATS) ? 60 : 0);
				comp->SetMaxImagination(HasComponent(COMPONENT_TYPE_RACING_STATS) ? 60 : 0);
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

		m_Components.insert(std::make_pair(COMPONENT_TYPE_DESTROYABLE, comp));
	}

	/*if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_DESTROYABLE) > 0 || m_Character) {
		DestroyableComponent* comp = new DestroyableComponent();
		if (m_Character) comp->LoadFromXML(m_Character->GetXMLDoc());
		m_Components.push_back(std::make_pair(COMPONENT_TYPE_DESTROYABLE, comp));
	}*/

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_CHARACTER) > 0 || m_Character) {
		// Character Component always has a possessor component
		m_Components.insert(std::make_pair(COMPONENT_TYPE_POSSESSOR, new PossessorComponent(this)));
		CharacterComponent* comp = new CharacterComponent(this, m_Character);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_CHARACTER, comp));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_INVENTORY) > 0 || m_Character) {
		InventoryComponent* comp = nullptr;
		if (m_Character) comp = new InventoryComponent(this, m_Character->GetXMLDoc());
		else comp = new InventoryComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_INVENTORY, comp));
	}
	// if this component exists, then we initialize it. it's value is always 0
	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_ROCKET_LAUNCH_LUP, -1) != -1) {
		auto comp = new RocketLaunchLupComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_ROCKET_LAUNCH_LUP, comp));
	}

	/**
	 * This is a bit of a mess
	 */

	CDScriptComponentTable* scriptCompTable = CDClientManager::Instance()->GetTable<CDScriptComponentTable>("ScriptComponent");
	int scriptComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SCRIPT);

	std::string scriptName = "";
	bool client = false;
	if (scriptComponentID > 0 || m_Character) {
		std::string clientScriptName;
		if (!m_Character) {
			CDScriptComponent scriptCompData = scriptCompTable->GetByID(scriptComponentID);
			scriptName = scriptCompData.script_name;
			clientScriptName = scriptCompData.client_script_name;
		}
		else {
			scriptName = "";
		}

		if (scriptName != "" || (scriptName == "" && m_Character)) {
			
		}
		else if (clientScriptName != "") {
			client = true;
		}
		else if (!m_Character) {
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

	if (!scriptName.empty() || client || m_Character) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_SCRIPT, new ScriptComponent(this, scriptName, true, client && scriptName.empty())));
	}

	// ZoneControl script
	if (m_TemplateID == 2365) {
		CDZoneTableTable* zoneTable = CDClientManager::Instance()->GetTable<CDZoneTableTable>("ZoneTable");
		const auto zoneID = dZoneManager::Instance()->GetZoneID();
		const CDZoneTable* zoneData = zoneTable->Query(zoneID.GetMapID());

		if (zoneData != nullptr)
		{
			int zoneScriptID = zoneData->scriptID;
			CDScriptComponent zoneScriptData = scriptCompTable->GetByID(zoneScriptID);

			ScriptComponent* comp = new ScriptComponent(this, zoneScriptData.script_name, true);
			m_Components.insert(std::make_pair(COMPONENT_TYPE_SCRIPT, comp));
		}
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SKILL, -1) != -1 || m_Character) {
		SkillComponent* comp = new SkillComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_SKILL, comp));
	}

	const auto combatAiId = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_BASE_COMBAT_AI);
	if (combatAiId > 0) {
		BaseCombatAIComponent* comp = new BaseCombatAIComponent(this, combatAiId);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_BASE_COMBAT_AI, comp));
	}

	if (int componentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_REBUILD) > 0) {
		RebuildComponent* comp = new RebuildComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_REBUILD, comp));

		CDRebuildComponentTable* rebCompTable = CDClientManager::Instance()->GetTable<CDRebuildComponentTable>("RebuildComponent");
		std::vector<CDRebuildComponent> rebCompData = rebCompTable->Query([=](CDRebuildComponent entry) { return (entry.id == rebuildComponentID); });

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

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SWITCH, -1) != -1) {
		SwitchComponent* comp = new SwitchComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_SWITCH, comp));
	}

	if ((compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_VENDOR) > 0)) {
		VendorComponent* comp = new VendorComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_VENDOR, comp));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PROPERTY_VENDOR, -1) != -1)
	{
		auto* component = new PropertyVendorComponent(this);
		m_Components.insert_or_assign(COMPONENT_TYPE_PROPERTY_VENDOR, component);
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PROPERTY_MANAGEMENT, -1) != -1)
	{
		auto* component = new PropertyManagementComponent(this);
		m_Components.insert_or_assign(COMPONENT_TYPE_PROPERTY_MANAGEMENT, component);
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_BOUNCER, -1) != -1) { // you have to determine it like this because all bouncers have a componentID of 0
		BouncerComponent* comp = new BouncerComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_BOUNCER, comp));
	}

	if ((compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_RENDER) > 0 && m_TemplateID != 2365) || m_Character) {
		RenderComponent* render = new RenderComponent(this);
		m_Components.insert(std::make_pair(COMPONENT_TYPE_RENDER, render));
	}

	if ((compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_MISSION_OFFER) > 0) || m_Character) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_MISSION_OFFER, new MissionOfferComponent(this, m_TemplateID)));
	}

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_BUILD_BORDER, -1) != -1) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_BUILD_BORDER, new BuildBorderComponent(this)));
	}

	// Scripted activity component
	int scriptedActivityID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SCRIPTED_ACTIVITY);
	if ((scriptedActivityID > 0)) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_SCRIPTED_ACTIVITY, new ScriptedActivityComponent(this, scriptedActivityID)));
	}

    if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_MODEL, -1) != -1 && !GetComponent<PetComponent>()) {
        m_Components.insert(std::make_pair(COMPONENT_TYPE_MODEL, new ModelComponent(this)));
		if (m_Components.find(COMPONENT_TYPE_DESTROYABLE) == m_Components.end()) {
			auto destroyableComponent = new DestroyableComponent(this);
			destroyableComponent->SetHealth(1);
			destroyableComponent->SetMaxHealth(1.0f);
			destroyableComponent->SetFaction(-1, true);
			destroyableComponent->SetIsSmashable(true);
			m_Components.insert(std::make_pair(COMPONENT_TYPE_DESTROYABLE, destroyableComponent));
		}
    }

	PetComponent* petComponent;
	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_ITEM) > 0 && !TryGetComponent(COMPONENT_TYPE_PET, petComponent) && !HasComponent(COMPONENT_TYPE_MODEL)) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_ITEM, nullptr));
	}

	// Shooting gallery component
    if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_SHOOTING_GALLERY) > 0) {
        m_Components.insert(std::make_pair(COMPONENT_TYPE_SHOOTING_GALLERY, new ShootingGalleryComponent(this)));
    }

	if (compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PROPERTY, -1) != -1) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_PROPERTY, new PropertyComponent(this)));
	}

	const int rocketId = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_ROCKET_LAUNCH);
	if ((rocketId > 0)) {
		m_Components.insert(std::make_pair(COMPONENT_TYPE_ROCKET_LAUNCH, new RocketLaunchpadControlComponent(this, rocketId)));
	}

	const int32_t railComponentID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_RAIL_ACTIVATOR);
	if (railComponentID > 0) {
	    m_Components.insert(std::make_pair(COMPONENT_TYPE_RAIL_ACTIVATOR, new RailActivatorComponent(this, railComponentID)));
	}

	int movementAIID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_MOVEMENT_AI);
	if (movementAIID > 0) {
		CDMovementAIComponentTable* moveAITable = CDClientManager::Instance()->GetTable<CDMovementAIComponentTable>("MovementAIComponent");
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

			m_Components.insert(std::make_pair(COMPONENT_TYPE_MOVEMENT_AI, new MovementAIComponent(this, moveInfo)));
		}
	}
	else if (petComponentId > 0 || combatAiId > 0 && GetComponent<BaseCombatAIComponent>()->GetTetherSpeed() > 0)
	{
		MovementAIInfo moveInfo = MovementAIInfo();
		moveInfo.movementType = "";
		moveInfo.wanderChance = 0;
		moveInfo.wanderRadius = 16;
		moveInfo.wanderSpeed = 2.5f;
		moveInfo.wanderDelayMax = 5;
		moveInfo.wanderDelayMin = 2;

		m_Components.insert(std::make_pair(COMPONENT_TYPE_MOVEMENT_AI, new MovementAIComponent(this, moveInfo)));
	}

	int proximityMonitorID = compRegistryTable->GetByIDAndType(m_TemplateID, COMPONENT_TYPE_PROXIMITY_MONITOR);
	if (proximityMonitorID > 0) {
		CDProximityMonitorComponentTable* proxCompTable = CDClientManager::Instance()->GetTable<CDProximityMonitorComponentTable>("ProximityMonitorComponent");
		std::vector<CDProximityMonitorComponent> proxCompData = proxCompTable->Query([=](CDProximityMonitorComponent entry) { return (entry.id == proximityMonitorID); });
		if (proxCompData.size() > 0) {
			std::vector<std::string> proximityStr = GeneralUtils::SplitString(proxCompData[0].Proximities, ',');
			ProximityMonitorComponent* comp = new ProximityMonitorComponent(this, std::stoi(proximityStr[0]), std::stoi(proximityStr[1]));
			m_Components.insert(std::make_pair(COMPONENT_TYPE_PROXIMITY_MONITOR, comp));
		}
	}

	// Hacky way to trigger these when the object has had a chance to get constructed
	AddCallbackTimer(0, [this]() {
		for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
			script->OnStartup(this);
		}
	});

	if (!m_Character && EntityManager::Instance()->GetGhostingEnabled())
	{
		// Don't ghost what is likely large scene elements
		if (m_Components.size() == 2 && HasComponent(COMPONENT_TYPE_SIMPLE_PHYSICS) && HasComponent(COMPONENT_TYPE_RENDER))
		{
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
			!HasComponent(COMPONENT_TYPE_SCRIPTED_ACTIVITY) && 
			!HasComponent(COMPONENT_TYPE_MOVING_PLATFORM) && 
			!HasComponent(COMPONENT_TYPE_PHANTOM_PHYSICS) && 
			!HasComponent(COMPONENT_TYPE_PROPERTY) && 
			!HasComponent(COMPONENT_TYPE_RACING_CONTROL) && 
			!HasComponent(COMPONENT_TYPE_VEHICLE_PHYSICS)
		)
		//if (HasComponent(COMPONENT_TYPE_BASE_COMBAT_AI))
		{
			m_IsGhostingCandidate = true;
		}

		if (GetLOT() == 6368)
		{
			m_IsGhostingCandidate = true;
		}

		// Special case for collectibles in Ninjago
		if (HasComponent(COMPONENT_TYPE_COLLECTIBLE) && Game::server->GetZoneID() == 2000)
		{
			m_IsGhostingCandidate = true;
		}
	}

	no_ghosting:

	TriggerEvent("OnCreate");

	if (m_Character) {
		auto* controllablePhysicsComponent = GetComponent<ControllablePhysicsComponent>();
		auto* characterComponent = GetComponent<CharacterComponent>();

		if (controllablePhysicsComponent != nullptr && characterComponent->GetLevel() >= 20)
		{
			controllablePhysicsComponent->SetSpeedMultiplier(525.0f / 500.0f);
		}
	}
}

bool Entity::operator==(const Entity& other) const {
	return other.m_ObjectID == m_ObjectID;
}

bool Entity::operator!=(const Entity& other) const {
	return other.m_ObjectID != m_ObjectID;
}

User* Entity::GetParentUser() const
{
	if (!IsPlayer())
	{
		return nullptr;
	}

	return static_cast<const Player*>(this)->GetParentUser();
}

Component* Entity::GetComponent(int32_t componentID) const {
	const auto& index = m_Components.find(componentID);

	if (index == m_Components.end())
	{
		return nullptr;
	}
	
	return index->second;
}

bool Entity::HasComponent(const int32_t componentId) const
{
	return m_Components.find(componentId) != m_Components.end();
}

void Entity::AddComponent(const int32_t componentId, Component* component)
{
	if (HasComponent(componentId))
	{
		return;
	}

	m_Components.insert_or_assign(componentId, component);
}

std::vector<ScriptComponent*> Entity::GetScriptComponents() {
	std::vector<ScriptComponent*> comps;
	for (std::pair<int32_t, void*> p : m_Components) {
		if (p.first == COMPONENT_TYPE_SCRIPT) {
			comps.push_back(static_cast<ScriptComponent*>(p.second));
		}
	}
	
	return comps;
}

void Entity::SetProximityRadius(float proxRadius, std::string name) {
	ProximityMonitorComponent* proxMon = GetComponent<ProximityMonitorComponent>();
	if (!proxMon) {
		proxMon = new ProximityMonitorComponent(this);
		m_Components.insert_or_assign(COMPONENT_TYPE_PROXIMITY_MONITOR, proxMon);
	}
	proxMon->SetProximityRadius(proxRadius, name);
}

void Entity::SetProximityRadius(dpEntity* entity, std::string name) {
	ProximityMonitorComponent* proxMon = GetComponent<ProximityMonitorComponent>();
	if (!proxMon) {
		proxMon = new ProximityMonitorComponent(this);
		m_Components.insert_or_assign(COMPONENT_TYPE_PROXIMITY_MONITOR, proxMon);
	}
	proxMon->SetProximityRadius(entity, name);
}

void Entity::SetGMLevel(uint8_t value) {
	m_GMLevel = value;
	if (GetParentUser()) {
		Character* character = GetParentUser()->GetLastUsedChar();

		if (character) {
			character->SetGMLevel(value);
		}
	}

	CharacterComponent* character = GetComponent<CharacterComponent>();
	if (character) character->SetGMLevel(value);

	GameMessages::SendGMLevelBroadcast(m_ObjectID, value);
}

void Entity::WriteBaseReplicaData(RakNet::BitStream* outBitStream, eReplicaPacketType packetType) {
	if (packetType == PACKET_TYPE_CONSTRUCTION) {
		outBitStream->Write(m_ObjectID);
		outBitStream->Write(m_TemplateID);

		if (IsPlayer()) {
			std::string name = m_Character != nullptr ? m_Character->GetName() : "Invalid";
			outBitStream->Write<uint8_t>(uint8_t(name.size()));

			for (size_t i = 0; i < name.size(); ++i) {
				outBitStream->Write<uint16_t>(name[i]);
			}
		}
		else {
			const auto& name = GetVar<std::string>(u"npcName");
			outBitStream->Write<uint8_t>(uint8_t(name.size()));

			for (size_t i = 0; i < name.size(); ++i) {
				outBitStream->Write<uint16_t>(name[i]);
			}
		}

		outBitStream->Write<uint32_t>(0); //Time since created on server

		const auto& syncLDF = GetVar<std::vector<std::u16string>>(u"syncLDF");

		// Only sync for models.
		if (m_Settings.size() > 0 && (GetComponent<ModelComponent>() && !GetComponent<PetComponent>())) {
			outBitStream->Write1(); //ldf data
			
			RakNet::BitStream settingStream;
			settingStream.Write<uint32_t>(m_Settings.size());

			for (LDFBaseData* data : m_Settings) {
				if (data) {
					data->WriteToPacket(&settingStream);
				}
			}

			outBitStream->Write(settingStream.GetNumberOfBytesUsed() + 1);
			outBitStream->Write<uint8_t>(0); //no compression used
			outBitStream->Write(settingStream);
		}
		else if (!syncLDF.empty()) {
			std::vector<LDFBaseData*> ldfData;

			for (const auto& data : syncLDF) {
				ldfData.push_back(GetVarData(data));
			}

			outBitStream->Write1(); //ldf data

			RakNet::BitStream settingStream;
			settingStream.Write<uint32_t>(ldfData.size());

			for (LDFBaseData* data : ldfData) {
				if (data) {
					data->WriteToPacket(&settingStream);
				}
			}

			outBitStream->Write(settingStream.GetNumberOfBytesUsed() + 1);
			outBitStream->Write<uint8_t>(0); //no compression used
			outBitStream->Write(settingStream);
		}
		else {
			outBitStream->Write0(); //No ldf data 
		}

		if (m_Trigger != nullptr && m_Trigger->events.size() > 0) {
			outBitStream->Write1();
		}
		else {
			outBitStream->Write0();
		}

		if (m_ParentEntity != nullptr || m_SpawnerID != 0) {
			outBitStream->Write1();
			if (m_ParentEntity != nullptr) outBitStream->Write(GeneralUtils::SetBit(m_ParentEntity->GetObjectID(), OBJECT_BIT_CLIENT));
			else if (m_Spawner != nullptr && m_Spawner->m_Info.isNetwork) outBitStream->Write(m_SpawnerID);
			else outBitStream->Write(GeneralUtils::SetBit(m_SpawnerID, OBJECT_BIT_CLIENT));
		}
		else outBitStream->Write0();

		outBitStream->Write(m_HasSpawnerNodeID);
		if (m_HasSpawnerNodeID) outBitStream->Write(m_SpawnerNodeID);

		//outBitStream->Write0(); //Spawner node id

		if (m_Scale == 1.0f || m_Scale == 0.0f) outBitStream->Write0();
		else {
			outBitStream->Write1();
			outBitStream->Write(m_Scale);
		}

		outBitStream->Write0(); //ObjectWorldState

		if (m_GMLevel != 0) {
			outBitStream->Write1();
			outBitStream->Write(m_GMLevel);
		}
		else outBitStream->Write0(); //No GM Level
	}
	
	// Only serialize parent / child info should the info be dirty (changed) or if this is the construction of the entity.
	outBitStream->Write(m_IsParentChildDirty || packetType == PACKET_TYPE_CONSTRUCTION);
	if (m_IsParentChildDirty || packetType == PACKET_TYPE_CONSTRUCTION) {
		m_IsParentChildDirty = false;
		outBitStream->Write(m_ParentEntity != nullptr);
		if (m_ParentEntity) {
			outBitStream->Write(m_ParentEntity->GetObjectID());
			outBitStream->Write0();
		}
		outBitStream->Write(m_ChildEntities.size() > 0);
		if (m_ChildEntities.size() > 0) {
			outBitStream->Write((uint16_t)m_ChildEntities.size());
			for (Entity* child : m_ChildEntities) {
				outBitStream->Write((uint64_t)child->GetObjectID());
			}
		}
	}
}

void Entity::WriteComponents(RakNet::BitStream* outBitStream, eReplicaPacketType packetType) {

	/**
	 * This has to be done in a specific order.
	 */

	bool destroyableSerialized = false;
	bool bIsInitialUpdate = false;
	if (packetType == PACKET_TYPE_CONSTRUCTION) bIsInitialUpdate = true;
	unsigned int flags = 0;

	PossessableComponent* possessableComponent;
	if (TryGetComponent(COMPONENT_TYPE_POSSESSABLE, possessableComponent))
	{
		possessableComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	ModuleAssemblyComponent* moduleAssemblyComponent;
	if (TryGetComponent(COMPONENT_TYPE_MODULE_ASSEMBLY, moduleAssemblyComponent))
	{
		moduleAssemblyComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}
	
	ControllablePhysicsComponent* controllablePhysicsComponent;
	if (TryGetComponent(COMPONENT_TYPE_CONTROLLABLE_PHYSICS, controllablePhysicsComponent))
	{
		controllablePhysicsComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	SimplePhysicsComponent* simplePhysicsComponent;
	if (TryGetComponent(COMPONENT_TYPE_SIMPLE_PHYSICS, simplePhysicsComponent))
	{
		simplePhysicsComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	RigidbodyPhantomPhysicsComponent* rigidbodyPhantomPhysics;
	if (TryGetComponent(COMPONENT_TYPE_RIGID_BODY_PHANTOM_PHYSICS, rigidbodyPhantomPhysics))
	{
		rigidbodyPhantomPhysics->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	VehiclePhysicsComponent* vehiclePhysicsComponent;
	if (TryGetComponent(COMPONENT_TYPE_VEHICLE_PHYSICS, vehiclePhysicsComponent))
	{
		vehiclePhysicsComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	PhantomPhysicsComponent* phantomPhysicsComponent;
	if (TryGetComponent(COMPONENT_TYPE_PHANTOM_PHYSICS, phantomPhysicsComponent))
	{
		phantomPhysicsComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	SoundTriggerComponent* soundTriggerComponent;
	if (TryGetComponent(COMPONENT_TYPE_SOUND_TRIGGER, soundTriggerComponent)) {
	    soundTriggerComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	BuffComponent* buffComponent;
	if (TryGetComponent(COMPONENT_TYPE_BUFF, buffComponent))
	{
		buffComponent->Serialize(outBitStream, bIsInitialUpdate, flags);

		DestroyableComponent* destroyableComponent;
		if (TryGetComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent))
		{
			destroyableComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
		}
		destroyableSerialized = true;
	}

	if (HasComponent(COMPONENT_TYPE_COLLECTIBLE)) {
		DestroyableComponent* destroyableComponent;
		if (TryGetComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent) && !destroyableSerialized)
		{
			destroyableComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
		}
		destroyableSerialized = true;
		outBitStream->Write(m_CollectibleID); // Collectable component
	}

	PetComponent* petComponent;
	if (TryGetComponent(COMPONENT_TYPE_PET, petComponent))
	{
		petComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	CharacterComponent* characterComponent;
	if (TryGetComponent(COMPONENT_TYPE_CHARACTER, characterComponent)) {

		PossessorComponent* possessorComponent;
		if (TryGetComponent(COMPONENT_TYPE_POSSESSOR, possessorComponent)) {
			possessorComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
		} else {
			// Should never happen, but just to be safe
			outBitStream->Write0();
		}
		characterComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	if (HasComponent(COMPONENT_TYPE_ITEM))
	{
		outBitStream->Write0();
	}

	InventoryComponent* inventoryComponent;
	if (TryGetComponent(COMPONENT_TYPE_INVENTORY, inventoryComponent))
	{
		inventoryComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	ScriptComponent* scriptComponent;
	if (TryGetComponent(COMPONENT_TYPE_SCRIPT, scriptComponent))
	{
		scriptComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	SkillComponent* skillComponent;
	if (TryGetComponent(COMPONENT_TYPE_SKILL, skillComponent))
	{
		skillComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	BaseCombatAIComponent* baseCombatAiComponent;
	if (TryGetComponent(COMPONENT_TYPE_BASE_COMBAT_AI, baseCombatAiComponent))
	{
		baseCombatAiComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	RebuildComponent* rebuildComponent;
	if (TryGetComponent(COMPONENT_TYPE_REBUILD, rebuildComponent))
	{
		DestroyableComponent* destroyableComponent;
		if (TryGetComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent) && !destroyableSerialized)
		{
			destroyableComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
		}
		destroyableSerialized = true;
		rebuildComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	MovingPlatformComponent* movingPlatformComponent;
	if (TryGetComponent(COMPONENT_TYPE_MOVING_PLATFORM, movingPlatformComponent))
	{
		movingPlatformComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	SwitchComponent* switchComponent;
	if (TryGetComponent(COMPONENT_TYPE_SWITCH, switchComponent)) {
		switchComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	VendorComponent* vendorComponent;
	if (TryGetComponent(COMPONENT_TYPE_VENDOR, vendorComponent))
	{
		vendorComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	BouncerComponent* bouncerComponent;
	if (TryGetComponent(COMPONENT_TYPE_BOUNCER, bouncerComponent))
	{
		bouncerComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	ScriptedActivityComponent* scriptedActivityComponent;
	if (TryGetComponent(COMPONENT_TYPE_SCRIPTED_ACTIVITY, scriptedActivityComponent)) {
		scriptedActivityComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

    ShootingGalleryComponent* shootingGalleryComponent;
    if (TryGetComponent(COMPONENT_TYPE_SHOOTING_GALLERY, shootingGalleryComponent)) {
        shootingGalleryComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
    }

	RacingControlComponent* racingControlComponent;
	if (TryGetComponent(COMPONENT_TYPE_RACING_CONTROL, racingControlComponent))
	{
		racingControlComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	LUPExhibitComponent* lupExhibitComponent;
	if (TryGetComponent(COMPONENT_TYPE_EXHIBIT, lupExhibitComponent))
	{
		lupExhibitComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	ModelComponent* modelComponent;
	if (TryGetComponent(COMPONENT_TYPE_MODEL, modelComponent)) {
		modelComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	RenderComponent* renderComponent;
	if (TryGetComponent(COMPONENT_TYPE_RENDER, renderComponent))
	{
		renderComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
	}

	if (modelComponent) {
		DestroyableComponent* destroyableComponent;
		if (TryGetComponent(COMPONENT_TYPE_DESTROYABLE, destroyableComponent) && !destroyableSerialized) {
			destroyableComponent->Serialize(outBitStream, bIsInitialUpdate, flags);
			destroyableSerialized = true;
		}
	}

	if (HasComponent(COMPONENT_TYPE_ZONE_CONTROL))
	{
		outBitStream->Write<uint32_t>(0x40000000);
	}

	// BBB Component, unused currently
	// Need to to write0 so that is serialized correctly
	// TODO: Implement BBB Component
	outBitStream->Write0();
}

void Entity::ResetFlags() {
	// Unused
}

void Entity::UpdateXMLDoc(tinyxml2::XMLDocument* doc) {
	//This function should only ever be called from within Character, meaning doc should always exist when this is called.
	//Naturally, we don't include any non-player components in this update function.

	for (const auto& pair : m_Components)
	{
		if (pair.second == nullptr) continue;

		pair.second->UpdateXml(doc);
	}
}

void Entity::Update(const float deltaTime) {
	uint32_t timerPosition;
	timerPosition = 0;
	while (timerPosition < m_Timers.size()) {
		m_Timers[timerPosition]->Update(deltaTime);
		if (m_Timers[timerPosition]->GetTime() <= 0) {
			const auto timerName = m_Timers[timerPosition]->GetName();

			delete m_Timers[timerPosition];
			m_Timers.erase(m_Timers.begin() + timerPosition);

			for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
				script->OnTimerDone(this, timerName);
			}
		} else {
			timerPosition++;
		}
	}

	for (int i = 0; i < m_CallbackTimers.size(); i++) {
		m_CallbackTimers[i]->Update(deltaTime);
		if (m_CallbackTimers[i]->GetTime() <= 0) {
			m_CallbackTimers[i]->GetCallback()();
			delete m_CallbackTimers[i];
			m_CallbackTimers.erase(m_CallbackTimers.begin() + i);
		}
	}
	
	// Add pending timers to the list of timers so they start next tick.
	if (m_PendingTimers.size() > 0) {
		for (auto namedTimer : m_PendingTimers) {
			m_Timers.push_back(namedTimer);
		}
		m_PendingTimers.clear();
	}

	if (IsSleeping())
	{
		Sleep();

		return;
	}
	else
	{
		Wake();
	}

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnUpdate(this);
	}

	for (const auto& pair : m_Components)
	{
		if (pair.second == nullptr) continue;

		pair.second->Update(deltaTime);
	}

	if (m_ShouldDestroyAfterUpdate) {
		EntityManager::Instance()->DestroyEntity(this->GetObjectID());
	}
}

void Entity::OnCollisionProximity(LWOOBJID otherEntity, const std::string& proxName, const std::string& status) {
	Entity* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnProximityUpdate(this, other, proxName, status);
	}

	RocketLaunchpadControlComponent* rocketComp = GetComponent<RocketLaunchpadControlComponent>();
	if (!rocketComp) return;

	rocketComp->OnProximityUpdate(other, proxName, status);
}

void Entity::OnCollisionPhantom(const LWOOBJID otherEntity) {
	auto* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnCollisionPhantom(this, other);
	}

	for (const auto& callback: m_PhantomCollisionCallbacks) {
	    callback(other);
	}

	SwitchComponent* switchComp = GetComponent<SwitchComponent>();
	if (switchComp) {
		switchComp->EntityEnter(other);
	}

	TriggerEvent("OnEnter", other);

	// POI system
	const auto& poi = GetVar<std::u16string>(u"POI");

	if (!poi.empty()) {
		auto* missionComponent = other->GetComponent<MissionComponent>();
		
		if (missionComponent != nullptr)
		{
			missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_LOCATION, 0, 0, GeneralUtils::UTF16ToWTF8(poi));
		}
	}

	if (!other->GetIsDead())
	{
		auto* combat = GetComponent<BaseCombatAIComponent>();
		
		if (combat != nullptr)
		{
			const auto index = std::find(m_TargetsInPhantom.begin(), m_TargetsInPhantom.end(), otherEntity);

			if (index != m_TargetsInPhantom.end()) return;

			const auto valid = combat->IsEnemy(otherEntity);

			if (!valid) return;

			m_TargetsInPhantom.push_back(otherEntity);
		}
	}
}

void Entity::OnCollisionLeavePhantom(const LWOOBJID otherEntity)
{
	auto* other = EntityManager::Instance()->GetEntity(otherEntity);
	if (!other) return;

	TriggerEvent("OnLeave", other);

	SwitchComponent* switchComp = GetComponent<SwitchComponent>();
	if (switchComp) {
		switchComp->EntityLeave(other);
	}

	const auto index = std::find(m_TargetsInPhantom.begin(), m_TargetsInPhantom.end(), otherEntity);
	
	if (index == m_TargetsInPhantom.end()) return;

	m_TargetsInPhantom.erase(index);
}

void Entity::OnFireEventServerSide(Entity* sender, std::string args, int32_t param1, int32_t param2, int32_t param3) {
    for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
        script->OnFireEventServerSide(this, sender, args, param1, param2, param3);
	}
}

void Entity::OnActivityStateChangeRequest(LWOOBJID senderID, int32_t value1, int32_t value2, const std::u16string& stringValue) {
    for (CppScripts::Script *script : CppScripts::GetEntityScripts(this)) {
        script->OnActivityStateChangeRequest(this, senderID, value1, value2, stringValue);
	}
}

void Entity::OnCinematicUpdate(Entity *self, Entity *sender, eCinematicEvent event, const std::u16string &pathName,
                               float_t pathTime, float_t totalTime, int32_t waypoint) {
    for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
        script->OnCinematicUpdate(self, sender, event, pathName, pathTime, totalTime, waypoint);
    }
}

void Entity::NotifyObject(Entity* sender, const std::string& name, int32_t param1, int32_t param2) 
{
	GameMessages::SendNotifyObject(GetObjectID(), sender->GetObjectID(), GeneralUtils::ASCIIToUTF16(name), UNASSIGNED_SYSTEM_ADDRESS);

    for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
        script->OnNotifyObject(this, sender, name, param1, param2);
	}
}

void Entity::OnEmoteReceived(const int32_t emote, Entity* target)
{
	for (auto* script : CppScripts::GetEntityScripts(this))
	{
		script->OnEmoteReceived(this, emote, target);
	}
}

void Entity::OnUse(Entity* originator) {
	TriggerEvent("OnInteract");

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnUse(this, originator);
	}

	// component base class when
	
	for (const auto& pair : m_Components)
	{
		if (pair.second == nullptr) continue;

		pair.second->OnUse(originator);
	}
}

void Entity::OnHitOrHealResult(Entity* attacker, int32_t damage) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnHitOrHealResult(this, attacker, damage);
	}
}

void Entity::OnHit(Entity* attacker) {
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnHit(this, attacker);
	}
}

void Entity::OnZonePropertyEditBegin()
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyEditBegin(this);
	}
}

void Entity::OnZonePropertyEditEnd()
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyEditEnd(this);
	}
}

void Entity::OnZonePropertyModelEquipped()
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelEquipped(this);
	}
}

void Entity::OnZonePropertyModelPlaced(Entity* player)
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelPlaced(this, player);
	}
}

void Entity::OnZonePropertyModelPickedUp(Entity* player)
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelPickedUp(this, player);
	}
}

void Entity::OnZonePropertyModelRemoved(Entity* player)
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelRemoved(this, player);
	}
}

void Entity::OnZonePropertyModelRemovedWhileEquipped(Entity* player)
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelRemovedWhileEquipped(this, player);
	}
}

void Entity::OnZonePropertyModelRotated(Entity* player)
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnZonePropertyModelRotated(this, player);
	}
}

void Entity::OnMessageBoxResponse(Entity* sender, int32_t button, const std::u16string& identifier,  const std::u16string& userData) 
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnMessageBoxResponse(this, sender, button, identifier, userData);
	}
}

void Entity::OnChoiceBoxResponse(Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier) 
{
	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this)) {
		script->OnChoiceBoxResponse(this, sender, button, buttonIdentifier, identifier);
	}
}

void Entity::Smash(const LWOOBJID source, const eKillType killType, const std::u16string& deathType) 
{
	if (!m_PlayerIsReadyForUpdates) return;

	auto* destroyableComponent = GetComponent<DestroyableComponent>();
	if (destroyableComponent == nullptr)
	{
		Kill(EntityManager::Instance()->GetEntity(source));
		return;
	}

	destroyableComponent->Smash(source, killType, deathType);
}

void Entity::Kill(Entity* murderer) {
	if (!m_PlayerIsReadyForUpdates) return;

	for (const auto& cb : m_DieCallbacks) {
		cb();
	}

	m_DieCallbacks.clear();

	//OMAI WA MOU, SHINDERIU

	for (CppScripts::Script* script : CppScripts::GetEntityScripts(this))
	{
		script->OnDie(this, murderer);
	}

	if (m_Spawner != nullptr) 
	{
		m_Spawner->NotifyOfEntityDeath(m_ObjectID);
	}

	if (!IsPlayer())
	{
		EntityManager::Instance()->DestroyEntity(this);
	}
	
	const auto& grpNameQBShowBricks = GetVar<std::string>(u"grpNameQBShowBricks");

	if (!grpNameQBShowBricks.empty())
	{
		auto spawners = dZoneManager::Instance()->GetSpawnersByName(grpNameQBShowBricks);

		Spawner* spawner = nullptr;

		if (!spawners.empty())
		{
			spawner = spawners[0];
		}
		else
		{
			spawners = dZoneManager::Instance()->GetSpawnersInGroup(grpNameQBShowBricks);

			if (!spawners.empty())
			{
				spawner = spawners[0];
			}
		}

		if (spawner != nullptr)
		{
			spawner->Spawn();
		}
	}

    // Track a player being smashed
    auto* characterComponent = GetComponent<CharacterComponent>();
    if (characterComponent != nullptr) {
        characterComponent->UpdatePlayerStatistic(TimesSmashed);
    }

    // Track a player smashing something else
    if (murderer != nullptr) {
        auto* murdererCharacterComponent = murderer->GetComponent<CharacterComponent>();
        if (murdererCharacterComponent != nullptr) {
            murdererCharacterComponent->UpdatePlayerStatistic(SmashablesSmashed);
        }
    }
}

void Entity::AddDieCallback(const std::function<void()>& callback) {
	m_DieCallbacks.push_back(callback);
}

void Entity::AddCollisionPhantomCallback(const std::function<void(Entity* target)>& callback) {
    m_PhantomCollisionCallbacks.push_back(callback);
}

void Entity::AddRebuildCompleteCallback(const std::function<void(Entity* user)> &callback) const {
    auto* rebuildComponent = GetComponent<RebuildComponent>();
    if (rebuildComponent != nullptr) {
        rebuildComponent->AddRebuildCompleteCallback(callback);
    }
}

bool Entity::GetIsDead() const {
	DestroyableComponent* dest = GetComponent<DestroyableComponent>();
	if (dest && dest->GetArmor() == 0 && dest->GetHealth() == 0) return true;

	return false;
}

void Entity::AddLootItem(const Loot::Info& info) {
	if (!IsPlayer()) return;
	auto& droppedLoot = static_cast<Player*>(this)->GetDroppedLoot();
	droppedLoot.insert(std::make_pair(info.id, info));
}

void Entity::PickupItem(const LWOOBJID& objectID) {
	if (!IsPlayer()) return;
	InventoryComponent* inv = GetComponent<InventoryComponent>();
	if (!inv) return;

	CDObjectsTable* objectsTable = CDClientManager::Instance()->GetTable<CDObjectsTable>("Objects");
	
	auto& droppedLoot = static_cast<Player*>(this)->GetDroppedLoot();

	for (const auto& p : droppedLoot) {
		if (p.first == objectID) {
		    auto* characterComponent = GetComponent<CharacterComponent>();
		    if (characterComponent != nullptr) {
                characterComponent->TrackLOTCollection(p.second.lot);
		    }

			const CDObjects& object = objectsTable->GetByID(p.second.lot);
			if (object.id != 0 && object.type == "Powerup") {
				CDObjectSkillsTable* skillsTable = CDClientManager::Instance()->GetTable<CDObjectSkillsTable>("ObjectSkills");
				std::vector<CDObjectSkills> skills = skillsTable->Query([=](CDObjectSkills entry) {return (entry.objectTemplate == p.second.lot); });
				for (CDObjectSkills skill : skills) {
					CDSkillBehaviorTable* skillBehTable = CDClientManager::Instance()->GetTable<CDSkillBehaviorTable>("SkillBehavior");
					CDSkillBehavior behaviorData = skillBehTable->GetSkillByID(skill.skillID);

					SkillComponent::HandleUnmanaged(behaviorData.behaviorID, GetObjectID());

					auto* missionComponent = GetComponent<MissionComponent>();

					if (missionComponent != nullptr)
					{
						missionComponent->Progress(MissionTaskType::MISSION_TASK_TYPE_POWERUP, skill.skillID);
					}
				}
			}
			else {
				inv->AddItem(p.second.lot, p.second.count, eLootSourceType::LOOT_SOURCE_PICKUP, eInventoryType::INVALID, {}, LWOOBJID_EMPTY, true, false, LWOOBJID_EMPTY, eInventoryType::INVALID, 1);
			}
		}
	}

	droppedLoot.erase(objectID);
}

bool Entity::CanPickupCoins(uint64_t count) {
	if (!IsPlayer()) return false;
	auto* player = static_cast<Player*>(this);
	auto droppedCoins = player->GetDroppedCoins();
	if (count > droppedCoins) { 
		return false;
	} else {
		player->SetDroppedCoins(droppedCoins - count);
		return true;
	}
}

void Entity::RegisterCoinDrop(uint64_t count) {
	if (!IsPlayer()) return;
	auto* player = static_cast<Player*>(this);
	auto droppedCoins = player->GetDroppedCoins();
	droppedCoins += count;
	player->SetDroppedCoins(droppedCoins);
}

void Entity::AddChild(Entity* child) {
	m_IsParentChildDirty = true;
	m_ChildEntities.push_back(child);
}

void Entity::RemoveChild(Entity* child) {
	if (!child) return;
	uint32_t entityPosition = 0;
	while (entityPosition < m_ChildEntities.size()) {
		if (!m_ChildEntities[entityPosition] || (m_ChildEntities[entityPosition])->GetObjectID() == child->GetObjectID()) {
			m_IsParentChildDirty = true;
			m_ChildEntities.erase(m_ChildEntities.begin() + entityPosition);
		} else {
			entityPosition++;
		}
	}
}

void Entity::RemoveParent() {
	this->m_ParentEntity = nullptr;
}

void Entity::AddTimer(std::string name, float time) {
	EntityTimer* timer = new EntityTimer(name, time);
	m_PendingTimers.push_back(timer);
}

void Entity::AddCallbackTimer(float time, std::function<void()> callback) {
	EntityCallbackTimer* timer = new EntityCallbackTimer(time, callback);
	m_CallbackTimers.push_back(timer);
}

bool Entity::HasTimer(const std::string& name) 
{
	for (auto* timer : m_Timers)
	{
		if (timer->GetName() == name)
		{
			return true;
		}
	}

	return false;
}

void Entity::CancelCallbackTimers() 
{
	for (auto* callback : m_CallbackTimers)
	{
		delete callback;
	}
	
	m_CallbackTimers.clear();
}

void Entity::ScheduleKillAfterUpdate(Entity* murderer) {
	//if (m_Info.spawner) m_Info.spawner->ScheduleKill(this);
	EntityManager::Instance()->ScheduleForKill(this);

	if (murderer) m_ScheduleKiller = murderer;
}

void Entity::CancelTimer(const std::string& name) {
    for (int i = 0; i < m_Timers.size(); i++) {
        if (m_Timers[i]->GetName() == name) {
            delete m_Timers[i];
            m_Timers.erase(m_Timers.begin() + i);
            return;
        }
    }
}

void Entity::CancelAllTimers() {
	/*for (auto timer : m_Timers) {
		if (timer) delete timer;
	}*/

	for (auto* timer : m_Timers)
	{
		delete timer;
	}

	m_Timers.clear();

	for (auto* callBackTimer : m_CallbackTimers) {
		delete callBackTimer;
	}

	m_CallbackTimers.clear();
}

bool Entity::IsPlayer() const {
	return m_TemplateID == 1 && GetSystemAddress() != UNASSIGNED_SYSTEM_ADDRESS;
}

void Entity::TriggerEvent(std::string eventID, Entity* optionalTarget) {
	if (m_Trigger != nullptr && m_Trigger->enabled) {
		for (LUTriggers::Event* triggerEvent : m_Trigger->events) {
			if (triggerEvent->eventID == eventID) {
				for (LUTriggers::Command* cmd : triggerEvent->commands) {
					HandleTriggerCommand(cmd->id, cmd->target, cmd->targetName, cmd->args, optionalTarget);
				}
			}
		}
	}
}

// This should probably get it's own triggers class at some point...
void Entity::HandleTriggerCommand(std::string id, std::string target, std::string targetName, std::string args, Entity* optionalTarget) {
	std::vector<std::string> argArray;
	// Parse args
	std::stringstream ssData(args);
	std::string token;
	char deliminator = ',';

	while (std::getline(ssData, token, deliminator)) {
		std::string lowerToken;
		for (char character : token) {
			lowerToken.push_back(std::tolower(character)); // make lowercase to ensure it works
		}
		argArray.push_back(lowerToken);
	}

	std::vector<Entity*> targetEntities;
	if (target == "self") targetEntities.push_back(this);
	if (target == "objGroup") targetEntities = EntityManager::Instance()->GetEntitiesInGroup(targetName);
	if (optionalTarget) targetEntities.push_back(optionalTarget);
	if (targetEntities.size() == 0) return;
	for (Entity* targetEntity : targetEntities) {
		if (!targetEntity) continue;

		if (id == "SetPhysicsVolumeEffect") {
			PhantomPhysicsComponent* phanPhys = GetComponent<PhantomPhysicsComponent>();
			if (!phanPhys) return;

			phanPhys->SetPhysicsEffectActive(true);
			uint32_t effectType = 0;
			if (argArray[0] == "push") effectType = 0;
			else if (argArray[0] == "attract") effectType = 1;
			else if (argArray[0] == "repulse") effectType = 2;
			else if (argArray[0] == "gravity") effectType = 3;
			else if (argArray[0] == "friction") effectType = 4;

			phanPhys->SetEffectType(effectType);
			phanPhys->SetDirectionalMultiplier(std::stof(argArray[1]));
			if (argArray.size() > 4) {
				NiPoint3 direction = NiPoint3::ZERO;
				GeneralUtils::TryParse<float>(argArray[2], direction.x);
				GeneralUtils::TryParse<float>(argArray[3], direction.y);
				GeneralUtils::TryParse<float>(argArray[4], direction.z);
				phanPhys->SetDirection(direction);
			}
			if (argArray.size() > 5) {
				phanPhys->SetMin(std::stoi(argArray[6]));
				phanPhys->SetMax(std::stoi(argArray[7]));
			}

			if (target == "self") {
				EntityManager::Instance()->ConstructEntity(this);
			}
		}
		else if (id == "updateMission") {
			CDMissionTasksTable* missionTasksTable = CDClientManager::Instance()->GetTable<CDMissionTasksTable>("MissionTasks");
			std::vector<CDMissionTasks> missionTasks = missionTasksTable->Query([=](CDMissionTasks entry) {
				std::string lowerTargetGroup;
				for (char character : entry.targetGroup) {
					lowerTargetGroup.push_back(std::tolower(character)); // make lowercase to ensure it works
				}
				
				return (lowerTargetGroup == argArray[4]);
			});

			for (const CDMissionTasks& task : missionTasks) {
				MissionComponent* missionComponent = targetEntity->GetComponent<MissionComponent>();
				if (!missionComponent) continue;

				missionComponent->ForceProgress(task.id, task.uid, std::stoi(argArray[2]));
			}
		}
		else if (id == "fireEvent") {
			for (CppScripts::Script* script : CppScripts::GetEntityScripts(targetEntity)) {
                script->OnFireEventServerSide(targetEntity, this, args, 0, 0, 0);
			}
		}
	}
}

Entity* Entity::GetOwner() const
{
	if (m_OwnerOverride != LWOOBJID_EMPTY)
	{
		auto* other = EntityManager::Instance()->GetEntity(m_OwnerOverride);

		if (other != nullptr)
		{
			return other->GetOwner();
		}
	}

	return const_cast<Entity*>(this);
}

const NiPoint3& Entity::GetDefaultPosition() const
{
	return m_DefaultPosition;
}

const NiQuaternion& Entity::GetDefaultRotation() const
{
	return m_DefaultRotation;
}

float Entity::GetDefaultScale() const
{
	return m_Scale;
}

void Entity::SetOwnerOverride(const LWOOBJID value)
{
	m_OwnerOverride = value;
}

bool Entity::GetIsGhostingCandidate() const
{
	return m_IsGhostingCandidate;
}

int8_t Entity::GetObservers() const
{
	return m_Observers;
}

void Entity::SetObservers(int8_t value) 
{
	if (value < 0)
	{
		value = 0;
	}

	m_Observers = value;
}

void Entity::Sleep() 
{
	auto* baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();
	
	if (baseCombatAIComponent != nullptr)
	{
		baseCombatAIComponent->Sleep();
	}	
}

void Entity::Wake() 
{
	auto* baseCombatAIComponent = GetComponent<BaseCombatAIComponent>();
	
	if (baseCombatAIComponent != nullptr)
	{
		baseCombatAIComponent->Wake();
	}
}

bool Entity::IsSleeping() const
{
	return m_IsGhostingCandidate && m_Observers == 0;
}


const NiPoint3& Entity::GetPosition() const
{
	if (!this) return NiPoint3::ZERO;

	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr)
	{
		return controllable->GetPosition();
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr)
	{
		return phantom->GetPosition();
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr)
	{
		return simple->GetPosition();
	}

	auto* vehicel = GetComponent<VehiclePhysicsComponent>();

	if (vehicel != nullptr)
	{
		return vehicel->GetPosition();
	}

	return NiPoint3::ZERO;
}

const NiQuaternion& Entity::GetRotation() const
{
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr)
	{
		return controllable->GetRotation();
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr)
	{
		return phantom->GetRotation();
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr)
	{
		return simple->GetRotation();
	}
	
	auto* vehicel = GetComponent<VehiclePhysicsComponent>();

	if (vehicel != nullptr)
	{
		return vehicel->GetRotation();
	}

	return NiQuaternion::IDENTITY;
}

void Entity::SetPosition(NiPoint3 position)
{
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr)
	{
		controllable->SetPosition(position);
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr)
	{
		phantom->SetPosition(position);
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr)
	{
		simple->SetPosition(position);
	}

	auto* vehicel = GetComponent<VehiclePhysicsComponent>();

	if (vehicel != nullptr)
	{
		vehicel->SetPosition(position);
	}

	EntityManager::Instance()->SerializeEntity(this);
}

void Entity::SetRotation(NiQuaternion rotation)
{
	auto* controllable = GetComponent<ControllablePhysicsComponent>();

	if (controllable != nullptr)
	{
		controllable->SetRotation(rotation);
	}

	auto* phantom = GetComponent<PhantomPhysicsComponent>();

	if (phantom != nullptr)
	{
		phantom->SetRotation(rotation);
	}

	auto* simple = GetComponent<SimplePhysicsComponent>();

	if (simple != nullptr)
	{
		simple->SetRotation(rotation);
	}

	auto* vehicel = GetComponent<VehiclePhysicsComponent>();

	if (vehicel != nullptr)
	{
		vehicel->SetRotation(rotation);
	}

	EntityManager::Instance()->SerializeEntity(this);
}

bool Entity::GetBoolean(const std::u16string& name) const
{
	return GetVar<bool>(name);
}

int32_t Entity::GetI32(const std::u16string& name) const
{
	return GetVar<int32_t>(name);
}

int64_t Entity::GetI64(const std::u16string& name) const
{
	return GetVar<int64_t>(name);
}

void Entity::SetBoolean(const std::u16string& name, const bool value)
{
	SetVar(name, value);
}

void Entity::SetI32(const std::u16string& name, const int32_t value)
{
	SetVar(name, value);
}

void Entity::SetI64(const std::u16string& name, const int64_t value)
{
	SetVar(name, value);
}

bool Entity::HasVar(const std::u16string& name) const
{
	for (auto* data : m_Settings)
	{
		if (data->GetKey() == name)
		{
			return true;
		}
	}
	
	return false;
}

uint16_t Entity::GetNetworkId() const
{
	return m_NetworkID;
}

void Entity::SetNetworkId(const uint16_t id)
{
	m_NetworkID = id;
}

std::vector<LWOOBJID>& Entity::GetTargetsInPhantom()
{
	std::vector<LWOOBJID> valid;

	// Clean up invalid targets, like disconnected players
	for (auto i = 0u; i < m_TargetsInPhantom.size(); ++i)
	{
		const auto id = m_TargetsInPhantom.at(i);

		auto* entity = EntityManager::Instance()->GetEntity(id);

		if (entity == nullptr)
		{
			continue;
		}

		valid.push_back(id);
	}

	m_TargetsInPhantom = valid;

	return m_TargetsInPhantom;
}

void Entity::SendNetworkVar(const std::string& data, const SystemAddress& sysAddr) {
    GameMessages::SendSetNetworkScriptVar(this, sysAddr, data);
}

LDFBaseData* Entity::GetVarData(const std::u16string& name) const
{
	for (auto* data : m_Settings)
	{
		if (data == nullptr)
		{
			continue;
		}

		if (data->GetKey() != name)
		{
			continue;
		}

		return data;
	}

	return nullptr;
}

std::string Entity::GetVarAsString(const std::u16string& name) const
{
	auto* data = GetVarData(name);

	if (data == nullptr)
	{
		return "";
	}

	return data->GetValueAsString();
}

void Entity::Resurrect() {
    if (IsPlayer()) {
        GameMessages::SendResurrect(this);
    }
}

void Entity::AddToGroup(const std::string& group) {
    if (std::find(m_Groups.begin(), m_Groups.end(), group) == m_Groups.end()) {
        m_Groups.push_back(group);
    }
}

void Entity::RetroactiveVaultSize() {
	auto inventoryComponent = GetComponent<InventoryComponent>();
	if (!inventoryComponent) return;

	auto itemsVault = inventoryComponent->GetInventory(eInventoryType::VAULT_ITEMS);
	auto modelVault = inventoryComponent->GetInventory(eInventoryType::VAULT_MODELS);

	if (itemsVault->GetSize() == modelVault->GetSize()) return;

	modelVault->SetSize(itemsVault->GetSize());
}
