#include "VanityUtilities.h"

#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "ObjectMessages.h"
#include "InventoryComponent.h"
#include "PhantomPhysicsComponent.h"
#include "ProximityMonitorComponent.h"
#include "ScriptComponent.h"
#include "dCommonVars.h"
#include "dConfig.h"
#include "dServer.h"
#include "Game.h"
#include "Logger.h"
#include "BinaryPathFinder.h"
#include "EntityInfo.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "ObjectIDManager.h"
#include "Level.h"
#include "VanityXml.h"
#include "VanityEvents.h"
#include "EventParts.h"
#include "Database.h"

#include <ctime>
#include <fstream>


namespace {
	std::vector<VanityObject> objects;
	LWOOBJID testament = LWOOBJID_EMPTY; // Nimbus Station's testament sign
}

void SetupNPCTalk(Entity* npc);
void NPCTalk(Entity* npc);
void AddObjects(const std::vector<VanityXml::Object>& documentObjects);
std::vector<VanityXml::Object> LoadVanity(const std::filesystem::path& folder);
LWOOBJID SpawnSpawner(const VanityObject& object, const VanityObjectLocation& location);
Entity* SpawnObject(const VanityObject& object, const VanityObjectLocation& location);
VanityObject* GetObject(const std::string& name);

void VanityUtilities::SpawnVanity() {
	const uint32_t zoneID = Game::server->GetZoneID();

	// Spawning again (the dashboard's reload) replaces what's there. Destroyed outright rather than smashed,
	// so players don't see the old ones linger through a death next to the new ones.
	Game::entityManager->DestroyEntity(Game::entityManager->GetEntity(testament));
	testament = LWOOBJID_EMPTY;

	if (zoneID == 1200) {
		{
			EntityInfo info;
			info.lot = 8139;
			info.pos = { 259.5f, 246.4f, -705.2f };
			info.rot = { 0.0f, 0.0f, 1.0f, 0.0f };
			info.spawnerID = Game::entityManager->GetZoneControlEntity()->GetObjectID();
			info.settings.Insert<bool>(u"hasCustomText", true);
			info.settings.Insert<std::string>(u"customText", ParseMarkdown((BinaryPathFinder::GetBinaryDir() / "vanity/TESTAMENT.md").string()));

			auto* entity = Game::entityManager->CreateEntity(info);
			Game::entityManager->ConstructEntity(entity);
			testament = entity->GetObjectID();
		}
	}

	for (const auto& npc : objects) {
		if (npc.m_ID == LWOOBJID_EMPTY) continue;
		if (npc.m_LOT == 176) {
			Game::zoneManager->RemoveSpawner(npc.m_ID);
		} else {
			Game::entityManager->DestroyEntity(Game::entityManager->GetEntity(npc.m_ID));
		}
	}

	objects.clear();

	if (Game::config->GetValue("disable_vanity") == "1") return;

	AddObjects(LoadVanity(BinaryPathFinder::GetBinaryDir() / "vanity"));

	// Loop through all objects
	for (auto& object : objects) {
		if (object.m_Locations.find(Game::server->GetZoneID()) == object.m_Locations.end()) continue;

		const std::vector<VanityObjectLocation>& locations = object.m_Locations.at(Game::server->GetZoneID());

		// Pick a random location
		const auto& location = locations[GeneralUtils::GenerateRandomNumber<int>(
			static_cast<size_t>(0), static_cast<size_t>(locations.size() - 1))];

		float rate = GeneralUtils::GenerateRandomNumber<float>(0, 1);
		if (location.m_Chance < rate) continue;

		if (object.m_LOT == 176) {
			object.m_ID = SpawnSpawner(object, location);
		} else {
			// Spawn the NPC
			auto* objectEntity = SpawnObject(object, location);
			if (!objectEntity) continue;
			object.m_ID = objectEntity->GetObjectID();
			if (!object.m_Phrases.empty()) {
				objectEntity->SetVar<std::vector<std::string>>(u"chats", object.m_Phrases);
				SetupNPCTalk(objectEntity);
			}
		}
	}
}

LWOOBJID SpawnSpawner(const VanityObject& object, const VanityObjectLocation& location) {
	SceneObject obj{};
	obj.lot = object.m_LOT;
	// guratantee we have no collisions
	do {
		obj.id = ObjectIDManager::GenerateObjectID();
	} while (Game::zoneManager->GetSpawner(obj.id));
	obj.position = location.m_Position;
	obj.rotation = location.m_Rotation;
	obj.settings = object.m_Config;
	Level::MakeSpawner(obj);
	return obj.id;
}

Entity* SpawnObject(const VanityObject& object, const VanityObjectLocation& location) {
	EntityInfo info;
	info.lot = object.m_LOT;
	info.pos = location.m_Position;
	info.rot = location.m_Rotation;
	info.scale = location.m_Scale;
	info.spawnerID = Game::entityManager->GetZoneControlEntity()->GetObjectID();
	info.settings = object.m_Config;

	auto* entity = Game::entityManager->CreateEntity(info);
	if (!object.m_Name.empty()) entity->SetVar(u"npcName", object.m_Name);
	if (entity->GetVar<bool>(u"noGhosting")) entity->SetIsGhostingCandidate(false);

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();
	if (inventoryComponent && !object.m_Equipment.empty()) {
		inventoryComponent->SetNPCItems(object.m_Equipment);
	}

	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
	if (destroyableComponent) {
		destroyableComponent->SetIsGMImmune(true);
		destroyableComponent->SetMaxHealth(0);
		destroyableComponent->SetHealth(0);
	}

	Game::entityManager->ConstructEntity(entity);

	return entity;
}

// root.xml and the files it switches on, with the vanity parts of the scheduled events that are on now: their file
// switches, then their overlays
std::vector<VanityXml::Object> LoadVanity(const std::filesystem::path& folder) {
	std::vector<IServerOperations::ScheduledEvent> events;
	try {
		events = Database::Get()->GetScheduledEvents();
	} catch (const std::exception& ex) {
		LOG("Could not read the scheduled events, spawning without their vanity changes: %s", ex.what());
	}
	const auto now = static_cast<int64_t>(std::time(nullptr));
	std::erase_if(events, [now](const auto& event) {
		return !ScheduleRules::IsOn(static_cast<ScheduleRules::eMode>(event.mode), event.schedule, event.startsAt, event.endsAt, now);
	});
	VanityEvents::SortForMerge(events);
	std::vector<VanityEvents::Changes> changes;
	for (const auto& event : events) {
		for (auto& change : EventParts::VanityChanges(event.name, event.parts)) {
			LOG("Vanity changes of event %s are on", event.name.c_str());
			changes.push_back(std::move(change));
		}
	}

	auto world = VanityEvents::LoadWorld(folder, "root.xml", changes);
	for (const auto& warning : world.warnings) LOG("Vanity: %s", warning.c_str());
	for (const auto& conflict : world.fileConflicts) {
		LOG("Vanity file %s is switched by more than one event; %s wins", conflict.file.c_str(), conflict.switches.back().first.c_str());
	}
	for (const auto& conflict : world.conflicts) {
		LOG("Vanity NPC %s is changed by more than one event; %s wins", conflict.npc.c_str(), conflict.events.back().c_str());
	}
	return std::move(world.objects);
}

void AddObjects(const std::vector<VanityXml::Object>& documentObjects) {
	// Read the objects
	const uint32_t currentZoneID = Game::server->GetZoneID();
	for (const auto& object : documentObjects) {
		// for use later when adding to the vector of VanityObjects
		bool useLocationsAsRandomSpawnPoint = false;

		if (object.lot == LOT_NULL) {
			LOG("Failed to parse object lot");
			continue;
		}

		std::vector<std::u16string> keys = {};
		LwoNameValue config;
		for (const auto& data : object.config) {
			const auto& configData = config.ParseInsert(data);
			if (configData->GetKey() == u"useLocationsAsRandomSpawnPoint" && configData->GetValueType() == eLDFType::LDF_TYPE_BOOLEAN) {
				useLocationsAsRandomSpawnPoint = static_cast<const LDFData<bool>*>(configData.get())->GetValue();
				config.Erase(u"useLocationsAsRandomSpawnPoint");
				continue;
			}
			keys.push_back(configData->GetKey());
		}
		if (!keys.empty()) config.Insert<std::vector<std::u16string>>(u"syncLDF", keys);

		VanityObject objectData{
			.m_Name = object.name,
			.m_LOT = object.lot,
			.m_Equipment = object.equipment,
			.m_Phrases = object.phrases,
			.m_Config = config
		};

		for (const auto& location : object.locations) {
			if (location.zone != currentZoneID) {
				continue;
			}

			VanityObjectLocation locationData{
				.m_Chance = location.chance.value_or(1.0f),
				.m_Position = { location.x, location.y, location.z },
				.m_Rotation = { location.rw, location.rx, location.ry, location.rz },
				.m_Scale = location.scale.value_or(1.0f),
			};

			objectData.m_Locations[location.zone].push_back(locationData);

			if (!useLocationsAsRandomSpawnPoint) {
				objects.push_back(objectData);
				objectData.m_Locations.clear();
			}
		}

		if (useLocationsAsRandomSpawnPoint && !objectData.m_Locations.empty()) {
			objects.push_back(objectData);
		}
	}
}

VanityObject* VanityUtilities::GetObject(const std::string& name) {
	for (size_t i = 0; i < objects.size(); i++) {
		if (objects[i].m_Name == name) {
			return &objects[i];
		}
	}
	return nullptr;
}

std::string VanityUtilities::ParseMarkdown(const std::string& file) {
	// This function will read the file and return the content formatted as ASCII text.

	// Read the file into a string
	std::ifstream t(file);
	std::stringstream output;
	// If the file does not exist, return a useful error.
	if (!t.good()) {
		output << "File ";
		output << file.substr(file.rfind("/") + 1);
		output << " not found!\nContact your DarkflameServer admin\nor find the server source at https://github.com/DarkflameUniverse/DarkflameServer";
		return output.str();
	}

	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string fileContents = buffer.str();

	// Loop through all lines in the file.
	// Replace all instances of the markdown syntax with the corresponding HTML.
	// Only care about headers
	std::string line;
	std::stringstream ss;
	ss << fileContents;
	while (std::getline(ss, line)) {

#define TOSTRING(x) #x

#ifndef STRINGIFY
#define STRINGIFY(x) TOSTRING(x)
#endif
		// Replace "__TIMESTAMP__" with the __TIMESTAMP__
		GeneralUtils::ReplaceInString(line, "__TIMESTAMP__", __TIMESTAMP__);
		// Replace "__VERSION__" with the PROJECT_VERSION
		GeneralUtils::ReplaceInString(line, "__VERSION__", Game::projectVersion);
		// Replace "__SOURCE__" with SOURCE
		GeneralUtils::ReplaceInString(line, "__SOURCE__", Game::config->GetValue("source"));
		// Replace "__LICENSE__" with LICENSE
		GeneralUtils::ReplaceInString(line, "__LICENSE__", "AGPL-3.0");

		if (line.find("##") != std::string::npos) {
			// Add "&lt;font size=&apos;18&apos; color=&apos;#000000&apos;&gt;" before the header
			output << "<font size=\"14\" color=\"#000000\">";
			// Add the header without the markdown syntax
			output << line.substr(3);

			output << "</font>";
		} else if (line.find("#") != std::string::npos) {
			// Add "&lt;font size=&apos;18&apos; color=&apos;#000000&apos;&gt;" before the header
			output << "<font size=\"18\" color=\"#000000\">";
			// Add the header without the markdown syntax
			output << line.substr(2);

			output << "</font>";
		} else {
			output << line;
		}

		output << "\n";
	}

	return output.str();
}

void SetupNPCTalk(Entity* npc) {
	npc->AddCallbackTimer(15.0f, [npc]() { NPCTalk(npc); });

	npc->SetProximityRadius(20.0f, "talk");
}

void VanityUtilities::OnProximityUpdate(Entity* entity, Entity* other, const std::string& proxName, const std::string& name) {
	if (proxName != "talk") return;
	const auto* const proximityMonitorComponent = entity->GetComponent<ProximityMonitorComponent>();
	if (!proximityMonitorComponent) return;

	if (name == "ENTER" && !entity->HasTimer("talk")) {
		NPCTalk(entity);
	}
}

void VanityUtilities::OnTimerDone(Entity* npc, const std::string& name) {
	if (name == "talk") {
		const auto* const proximityMonitorComponent = npc->GetComponent<ProximityMonitorComponent>();
		if (!proximityMonitorComponent || proximityMonitorComponent->GetProximityObjects("talk").empty()) return;

		NPCTalk(npc);
	}
}

void NPCTalk(Entity* npc) {
	const auto& chats = npc->GetVar<std::vector<std::string>>(u"chats");

	if (chats.empty()) return;

	const auto& selected
		= chats[GeneralUtils::GenerateRandomNumber<int32_t>(0, static_cast<int32_t>(chats.size() - 1))];

	GameMessages::NotifyClientZoneObject(npc->GetObjectID(), u"sendToclient_bubble", 0, 0, npc->GetObjectID(), selected).Send(UNASSIGNED_SYSTEM_ADDRESS);

	Game::entityManager->SerializeEntity(npc);

	const float nextTime = GeneralUtils::GenerateRandomNumber<float>(15, 60);

	npc->AddTimer("talk", nextTime);
}
