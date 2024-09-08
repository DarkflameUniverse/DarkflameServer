#include "Scene.h"

#include <filesystem>

#include <tinyxml2.h>

#include "ServerPreconditions.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "MissionComponent.h"
#include "dConfig.h"
#include "PlayerManager.h"
#include "SlashCommandHandler.h"
#include "ChatPackets.h"
#include "InventoryComponent.h"

using namespace Cinema;

std::unordered_map<std::string, Scene> Scene::m_Scenes;

void Cinema::Scene::AddObject(LOT lot, NiPoint3 position, NiQuaternion rotation) {
	m_Objects.push_back(std::make_pair(lot, std::make_pair(position, rotation)));
}

void Scene::AddPrefab(const Prefab& prefab, NiPoint3 position) {
	m_Prefabs.push_back(std::make_pair(prefab, position));
}

void Cinema::Scene::AddNPC(LOT npc, const std::string& name, Recording::Recorder* act) {
	m_NPCs.push_back(std::make_pair(npc, std::make_pair(act, name)));
}

void Cinema::Scene::Rehearse() {
	CheckForShowings();
}

void Cinema::Scene::Conclude(Entity* player) {
	if (player == nullptr) {
		return;
	}

	if (m_Audience.find(player->GetObjectID()) == m_Audience.end()) {
		return;
	}

	auto* missionComponent = player->GetComponent<MissionComponent>();

	if (missionComponent == nullptr) {
		return;
	}

	if (m_CompleteMission != 0) {
		missionComponent->CompleteMission(m_CompleteMission);
	}

	if (m_AcceptMission != 0) {
		missionComponent->AcceptMission(m_AcceptMission);
	}

	// Remove the player from the audience
	m_Audience.erase(player->GetObjectID());
	m_HasBeenOutside.erase(player->GetObjectID());
	
	m_VisitedPlayers.emplace(player->GetObjectID());
}

bool Cinema::Scene::IsPlayerInBounds(Entity* player) const {
	if (player == nullptr) {
		return false;
	}

	if (m_Bounds == 0.0f) {
		return true;
	}

	const auto& position = player->GetPosition();
	
	auto distance = NiPoint3::Distance(position, m_Center);

	return distance <= m_Bounds;
}

bool Cinema::Scene::IsPlayerInShowingDistance(Entity* player) const {
	if (player == nullptr) {
		return false;
	}

	if (m_ShowingDistance == 0.0f) {
		return true;
	}

	const auto& position = player->GetPosition();

	auto distance = NiPoint3::Distance(position, m_Center);

	return distance <= m_ShowingDistance;
}

bool Cinema::Scene::IsPlayerInMaximumShowingDistance(Entity* player) const {
	if (player == nullptr) {
		return false;
	}

	if (m_MaximumShowingDistance == 0.0f) {
		return true;
	}

	const auto& position = player->GetPosition();

	auto distance = NiPoint3::Distance(position, m_Center);

	return distance <= m_MaximumShowingDistance;
}

void Cinema::Scene::AutoLoadScenesForZone(LWOMAPID zone) {
	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "record-act" },
		.handle = CommandRecordAct,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "record-start" },
		.handle = CommandRecordStart,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "record-stop" },
		.handle = CommandRecordStop,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "record-save" },
		.handle = CommandRecordSave,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "record-load" },
		.handle = CommandRecordLoad,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "prefab-spawn" },
		.handle = CommandPrefabSpawn,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "prefab-destroy" },
		.handle = CommandPrefabDestroy,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "scene-act" },
		.handle = CommandSceneAct,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	SlashCommandHandler::RegisterCommand(Command{
		.help = "",
		.info = "",
		.aliases = { "scene-setup" },
		.handle = CommandSceneSetup,
		.requiredLevel = eGameMasterLevel::LEAD_MODERATOR
	});

	const auto& scenesRoot = Game::config->GetValue("scenes_directory");

	if (scenesRoot.empty()) {
		return;
	}

	const auto path = std::filesystem::path(scenesRoot) / std::to_string(zone);

	if (!std::filesystem::exists(path)) {
		return;
	}

	// Recursively iterate through the directory
	for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		// Check that extension is .xml
		if (entry.path().extension() != ".xml") {
			continue;
		}

		const auto& file = entry.path().string();

		auto& scene = LoadFromFile(file);

		if (scene.m_ChanceToPlay != 1.0f) {
			const auto chance = GeneralUtils::GenerateRandomNumber<float>(0.0f, 1.0f);

			if (chance > scene.m_ChanceToPlay) {
				continue;
			}
		}

		scene.Rehearse();
	}
}

void Cinema::Scene::CheckForShowings() {
	auto audience = m_Audience;
	auto hasBeenOutside = m_HasBeenOutside;

	for (const auto& member : audience) {
		if (Game::entityManager->GetEntity(member) == nullptr) {
			m_Audience.erase(member);
		}
	}

	for (const auto& member : hasBeenOutside) {
		if (Game::entityManager->GetEntity(member) == nullptr) {
			m_HasBeenOutside.erase(member);
		}
	}

	m_Audience = audience;
	m_HasBeenOutside = hasBeenOutside;

	// I don't care
	Game::entityManager->GetZoneControlEntity()->AddCallbackTimer(1.0f, [this]() {
		for (auto* player : PlayerManager::GetAllPlayers()) {
			if (m_Audience.find(player->GetObjectID()) != m_Audience.end()) {
				continue;
			}

			if (!m_Repeatable && m_VisitedPlayers.find(player->GetObjectID()) != m_VisitedPlayers.end()) {
				continue;
			}

			CheckTicket(player);
		}

		CheckForShowings();
	});
}

void Cinema::Scene::CheckTicket(Entity* player) {
	if (m_Audience.find(player->GetObjectID()) != m_Audience.end()) {
		return;
	}

	for (const auto& [expression, invert] : m_Preconditions) {
		if (expression.Check(player) == invert) {
			return;
		}
	}

	if (!IsPlayerInShowingDistance(player)) {
		m_HasBeenOutside.emplace(player->GetObjectID());

		return;
	}

	if (m_HasBeenOutside.find(player->GetObjectID()) == m_HasBeenOutside.end()) {
		return;
	}

	m_Audience.emplace(player->GetObjectID());

	Act(player);
}

Play* Cinema::Scene::Act(Entity* player) {
	auto* play = new Play();

	if (player != nullptr) {
		play->player = player->GetObjectID();
	}

	play->scene = this;

	for (const auto& [lot, transform] : m_Objects) {
		const auto& [position, rotation] = transform;
		EntityInfo info;
		info.pos = position;
		info.rot = rotation;
		info.lot = lot;
		info.spawnerID = Game::entityManager->GetZoneControlEntity()->GetObjectID();

		auto* entity = Game::entityManager->CreateEntity(info);

		Game::entityManager->ConstructEntity(entity);

		if (player != nullptr) {
			ServerPreconditions::AddSoloActor(entity->GetObjectID(), player->GetObjectID());
		}

		LOG("Spawing object %d", entity->GetObjectID());
	}

	for (const auto& [prefab, position] : m_Prefabs) {
		const auto instanceId = prefab.Instantiate(position);

		const auto& entities = prefab.GetEntities(instanceId);

		if (player != nullptr) {
			for (const auto& entity : entities) {
				ServerPreconditions::AddSoloActor(entity, player->GetObjectID());
			}
		}
		
		for (const auto& entity : entities) {
			play->entities.emplace(entity);
		}

		LOG("Spawing prefab %d", instanceId);
	}

	for (const auto& [npc, meta] : m_NPCs) {
		const auto& [act, name] = meta;
		EntityInfo info;
		info.pos = NiPoint3();
		info.rot = NiQuaternion();
		info.lot = npc;
		info.spawnerID = Game::entityManager->GetZoneControlEntity()->GetObjectID();
		info.settings = {
			new LDFData<std::vector<std::u16string>>(u"syncLDF", { u"custom_script_client" }),
			new LDFData<std::u16string>(u"custom_script_client", u"scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua")
		};

		auto* entity = Game::entityManager->CreateEntity(info);

		play->entities.emplace(entity->GetObjectID());

		Game::entityManager->ConstructEntity(entity);

		Entity* actor; 

		if (player != nullptr) {
			actor = act->ActFor(entity, player, play);
		}
		else
		{
			act->Act(entity, play);

			actor = entity;
		}

		if (actor != nullptr) {
			actor->SetVar(u"npcName", name);
		}

		play->entities.emplace(actor->GetObjectID());

		LOG("Spawing npc %d", entity->GetObjectID());
	}

	if (player != nullptr) {
		play->SetupCheckForAudience();
	}

	return play;
}

Scene& Cinema::Scene::LoadFromFile(std::string file) {
	if (m_Scenes.find(file) != m_Scenes.end()) {
		return m_Scenes[file];
	}

	Scene scene;

	tinyxml2::XMLDocument doc;
	doc.LoadFile(file.c_str());

	tinyxml2::XMLElement* root = doc.FirstChildElement("Scene");
	if (!root) {
		LOG("Failed to load scene from file: %s", file.c_str());

		m_Scenes.emplace(file, scene);

		return m_Scenes[file];
	}

	// Load center and bounds
	if (root->Attribute("x")) {
		scene.m_Center = NiPoint3(root->FloatAttribute("x"), root->FloatAttribute("y"), root->FloatAttribute("z"));
	}

	if (root->Attribute("performingDistance")) {
		scene.m_Bounds = root->FloatAttribute("performingDistance");
	}

	if (root->Attribute("showingDistance")) {
		scene.m_ShowingDistance = root->FloatAttribute("showingDistance");
	} else {
		scene.m_ShowingDistance = scene.m_Bounds * 2.0f;
	}

	if (root->Attribute("maximumShowingDistance")) {
		scene.m_MaximumShowingDistance = root->FloatAttribute("maximumShowingDistance");
	} else {
		scene.m_MaximumShowingDistance = scene.m_ShowingDistance * 2.0f;
	}

	if (root->Attribute("chanceToPlay")) {
		scene.m_ChanceToPlay = root->FloatAttribute("chanceToPlay");
	}

	if (root->Attribute("repeatable")) {
		scene.m_Repeatable = root->BoolAttribute("repeatable");
	}

	// Load accept and complete mission
	if (root->Attribute("acceptMission")) {
		scene.m_AcceptMission = root->IntAttribute("acceptMission");
	}

	if (root->Attribute("completeMission")) {
		scene.m_CompleteMission = root->IntAttribute("completeMission");
	}

	// Load preconditions
	for (tinyxml2::XMLElement* element = root->FirstChildElement("Precondition"); element; element = element->NextSiblingElement("Precondition")) {
		scene.m_Preconditions.push_back(std::make_pair(Preconditions::CreateExpression(element->Attribute("expression")), element->BoolAttribute("not")));
	}

	for (tinyxml2::XMLElement* element = root->FirstChildElement("Object"); element; element = element->NextSiblingElement("Object")) {
		const auto lot = element->UnsignedAttribute("lot");
		const auto position = NiPoint3(element->FloatAttribute("x"), element->FloatAttribute("y"), element->FloatAttribute("z"));
		const auto rotation = NiQuaternion(element->FloatAttribute("qx"), element->FloatAttribute("qy"), element->FloatAttribute("qz"), element->FloatAttribute("qw"));

		scene.AddObject(lot, position, rotation);
	}

	for (tinyxml2::XMLElement* element = root->FirstChildElement("Prefab"); element; element = element->NextSiblingElement("Prefab")) {
		const auto prefab = element->Attribute("file");
		const auto position = NiPoint3(element->FloatAttribute("x"), element->FloatAttribute("y"), element->FloatAttribute("z"));

		scene.AddPrefab(Prefab::LoadFromFile(prefab), position);
	}

	for (tinyxml2::XMLElement* element = root->FirstChildElement("NPC"); element; element = element->NextSiblingElement("NPC")) {
		LOT npc = 2097253;
		if (element->Attribute("lot")) {
			npc = element->UnsignedAttribute("lot");
		}

		std::string name = "";

		if (element->Attribute("name")) {
			name = element->Attribute("name");
		}
		
		const auto act = element->Attribute("act");

		scene.AddNPC(npc, name, Recording::Recorder::LoadFromFile(act));
	}

	LOG("Loaded scene from file: %s", file.c_str());

	m_Scenes.emplace(file, scene);

	return m_Scenes[file];
}

void Cinema::Scene::CommandRecordAct(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	const auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	EntityInfo info;
	info.lot = 0;
	info.pos = entity->GetPosition();
	info.rot = entity->GetRotation();
	info.scale = 1;
	info.spawner = nullptr;
	info.spawnerID = entity->GetObjectID();
	info.spawnerNodeID = 0;
	info.settings = {
		new LDFData<std::vector<std::u16string>>(u"syncLDF", { u"custom_script_client" }),
		new LDFData<std::u16string>(u"custom_script_client", u"scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua")
	};

	// If there is an argument, set the lot
	const auto lotOptional = GeneralUtils::TryParse<LOT>(splitArgs[0]);
	if (lotOptional) {
		info.lot = lotOptional.value();
	} else {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid lot.");
		return;
	}

	// Spawn it
	auto* actor = Game::entityManager->CreateEntity(info);

	// If there is an argument, set the actors name
	if (args.size() > 1) {
		actor->SetVar(u"npcName", args[1]);
	}

	// Construct it
	Game::entityManager->ConstructEntity(actor);

	auto* record = Cinema::Recording::Recorder::GetRecorder(entity->GetObjectID());

	if (record) {
		record->Act(actor);
	} else {
		LOG("Failed to get recorder for objectID: %llu", entity->GetObjectID());
	}
}

void Cinema::Scene::CommandRecordStart(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');

	Cinema::Recording::Recorder::StartRecording(entity->GetObjectID());

	auto* record = Cinema::Recording::Recorder::GetRecorder(entity->GetObjectID());

	if (record) {
		if (args.size() > 0 && splitArgs[0] == "clear") {
			record->AddRecord(new Cinema::Recording::ClearEquippedRecord());
		}
		else if (args.size() > 0 && splitArgs[0] == "copy") {
			record->AddRecord(new Cinema::Recording::ClearEquippedRecord());

			auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

			if (inventoryComponent) {
				for (const auto& [k, v] : inventoryComponent->GetEquippedItems()) {
					auto* equipmentRecord = new Cinema::Recording::EquipRecord();
					equipmentRecord->item = v.lot;

					record->AddRecord(equipmentRecord);
				}
			}
		}
	} else {
		LOG("Failed to get recorder for objectID: %llu", entity->GetObjectID());
	}
}

void Cinema::Scene::CommandRecordStop(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	Cinema::Recording::Recorder::StopRecording(entity->GetObjectID());
}

void Cinema::Scene::CommandRecordSave(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	auto* record = Cinema::Recording::Recorder::GetRecorder(entity->GetObjectID());

	if (record) {
		record->SaveToFile(splitArgs[0]);
	} else {
		LOG("Failed to get recorder for objectID: %llu", entity->GetObjectID());
	}
}

void Cinema::Scene::CommandRecordLoad(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	auto* record = Cinema::Recording::Recorder::LoadFromFile(splitArgs[0]);

	if (record) {
		Cinema::Recording::Recorder::AddRecording(entity->GetObjectID(), record);
	} else {
		LOG("Failed to load recording from file: %s", splitArgs[0].c_str());
	}
}

void Cinema::Scene::CommandPrefabSpawn(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	const auto& prefab = Cinema::Prefab::LoadFromFile(splitArgs[0]);

	float scale = 1.0f;

	if (args.size() >= 2) {
		const auto scaleOptional = GeneralUtils::TryParse<float>(splitArgs[1]);
		if (scaleOptional) {
			scale = scaleOptional.value();
		}
	}

	size_t id = prefab.Instantiate(entity->GetPosition(), scale);

	ChatPackets::SendSystemMessage(sysAddr, u"Spawned prefab with ID: " + GeneralUtils::to_u16string(id));
}

void Cinema::Scene::CommandPrefabDestroy(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	size_t id = GeneralUtils::TryParse<size_t>(splitArgs[0]).value_or(0);

	if (id == 0) {
		ChatPackets::SendSystemMessage(sysAddr, u"Invalid prefab ID.");
		return;
	}

	Cinema::Prefab::DestroyInstance(id);

	ChatPackets::SendSystemMessage(sysAddr, u"Destroyed prefab with ID: " + GeneralUtils::to_u16string(id));

	return;
}

void Cinema::Scene::CommandSceneAct(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	auto& scene = Cinema::Scene::LoadFromFile(splitArgs[0]);

	scene.Act(entity);
}

void Cinema::Scene::CommandSceneSetup(Entity* entity, const SystemAddress& sysAddr, const std::string args) {
	auto splitArgs = GeneralUtils::SplitString(args, ' ');
	if (splitArgs.empty()) return;

	auto& scene = Cinema::Scene::LoadFromFile(splitArgs[0]);

	scene.Rehearse();
}
