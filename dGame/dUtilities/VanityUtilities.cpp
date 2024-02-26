#include "VanityUtilities.h"

#include "DestroyableComponent.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "PhantomPhysicsComponent.h"
#include "ProximityMonitorComponent.h"
#include "ScriptComponent.h"
#include "dCommonVars.h"
#include "dConfig.h"
#include "dServer.h"
#include "tinyxml2.h"
#include "Game.h"
#include "Logger.h"
#include "BinaryPathFinder.h"
#include "EntityInfo.h"
#include "Spawner.h"
#include "dZoneManager.h"
#include "ObjectIDManager.h"
#include "Level.h"

#include <fstream>


namespace {
	std::vector<VanityObject> m_Objects;
	std::set<std::string> m_LoadedFiles;
}

void SetupNPCTalk(Entity* npc);
void NPCTalk(Entity* npc);
void ParseXml(const std::string& file);
LWOOBJID SpawnSpawner(const VanityObject& object, const VanityObjectLocation& location);
Entity* SpawnObject(const VanityObject& object, const VanityObjectLocation& location);
VanityObject* GetObject(const std::string& name);

void VanityUtilities::SpawnVanity() {
	const uint32_t zoneID = Game::server->GetZoneID();

	if (zoneID == 1200) {
		{
			EntityInfo info;
			info.lot = 8139;
			info.pos = { 259.5f, 246.4f, -705.2f };
			info.rot = { 0.0f, 0.0f, 1.0f, 0.0f };
			info.spawnerID = Game::entityManager->GetZoneControlEntity()->GetObjectID();
			info.settings = {
				new LDFData<bool>(u"hasCustomText", true),
				new LDFData<std::string>(u"customText", ParseMarkdown((BinaryPathFinder::GetBinaryDir() / "vanity/TESTAMENT.md").string()))
			};

			auto* entity = Game::entityManager->CreateEntity(info);
			Game::entityManager->ConstructEntity(entity);
		}
	}

	if (Game::config->GetValue("disable_vanity") == "1") return;

	for (const auto& npc : m_Objects) {
		if (npc.m_ID == LWOOBJID_EMPTY) continue;
		if (npc.m_LOT == 176){
			Game::zoneManager->RemoveSpawner(npc.m_ID);
		} else{
			auto* entity = Game::entityManager->GetEntity(npc.m_ID);
			if (!entity) continue;
			entity->Smash(LWOOBJID_EMPTY, eKillType::VIOLENT);
		}
	}

	m_Objects.clear();
	m_LoadedFiles.clear();

	ParseXml((BinaryPathFinder::GetBinaryDir() / "vanity/root.xml").string());

	// Loop through all objects
	for (auto& object : m_Objects) {
		if (object.m_Locations.find(Game::server->GetZoneID()) == object.m_Locations.end()) continue;

		const std::vector<VanityObjectLocation>& locations = object.m_Locations.at(Game::server->GetZoneID());

		// Pick a random location
		const auto& location = locations[GeneralUtils::GenerateRandomNumber<int>(
			static_cast<size_t>(0), static_cast<size_t>(locations.size() - 1))];

		float rate = GeneralUtils::GenerateRandomNumber<float>(0, 1);
		if (location.m_Chance < rate) continue;

		if (object.m_LOT == 176){
			object.m_ID = SpawnSpawner(object, location);
		} else {
			// Spawn the NPC
			auto* objectEntity = SpawnObject(object, location);
			if (!objectEntity) continue;
			object.m_ID = objectEntity->GetObjectID();
			if (!object.m_Phrases.empty()){
				objectEntity->SetVar<std::vector<std::string>>(u"chats", object.m_Phrases);
				SetupNPCTalk(objectEntity);
			}
		}
	}
}

LWOOBJID SpawnSpawner(const VanityObject& object, const VanityObjectLocation& location) {
	SceneObject obj;
	obj.lot = object.m_LOT;
	// guratantee we have no collisions
	do {
		obj.id = ObjectIDManager::GenerateObjectID();
	} while(Game::zoneManager->GetSpawner(obj.id));
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
	entity->SetVar(u"npcName", object.m_Name);
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

void ParseXml(const std::string& file) {
	if (m_LoadedFiles.contains(file)){
		LOG("Trying to load vanity file %s twice!!!", file.c_str());
		return;
	}
	m_LoadedFiles.insert(file);
	// Read the entire file
	std::ifstream xmlFile(file);
	std::string xml((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());

	// Parse the XML
	tinyxml2::XMLDocument doc;
	doc.Parse(xml.c_str(), xml.size());

	// Read the objects
	auto* files = doc.FirstChildElement("files");
	if (files) {
		for (auto* file = files->FirstChildElement("file"); file != nullptr; file = file->NextSiblingElement("file")) {
			std::string enabled = file->Attribute("enabled");
			std::string filename = file->Attribute("name");
			if (enabled != "1") {
				continue;
			}
			ParseXml((BinaryPathFinder::GetBinaryDir() / "vanity" / filename).string());
		}
	}

	// Read the objects
	auto* objects = doc.FirstChildElement("objects");

	if (objects) {
		for (auto* object = objects->FirstChildElement("object"); object != nullptr; object = object->NextSiblingElement("object")) {
			// for use later when adding to the vector of VanityObjects
			bool useLocationsAsRandomSpawnPoint = false;
			// Get the NPC name
			auto* name = object->Attribute("name");

			if (!name) name = "";

			// Get the NPC lot
			auto* lot = object->Attribute("lot");

			if (lot == nullptr) {
				LOG("Failed to parse object lot");
				continue;
			}

			// Get the equipment
			auto* equipment = object->FirstChildElement("equipment");
			std::vector<LOT> inventory;

			if (equipment) {
				auto* text = equipment->GetText();

				if (text != nullptr) {
					std::string equipmentString(text);

					std::vector<std::string> splitEquipment = GeneralUtils::SplitString(equipmentString, ',');

					for (auto& item : splitEquipment) {
						inventory.push_back(std::stoi(item));
					}
				}
			}


			// Get the phrases
			auto* phrases = object->FirstChildElement("phrases");

			std::vector<std::string> phraseList = {};

			if (phrases) {
				for (auto* phrase = phrases->FirstChildElement("phrase"); phrase != nullptr;
					phrase = phrase->NextSiblingElement("phrase")) {
					// Get the phrase
					auto* text = phrase->GetText();
					if (text == nullptr) {
						LOG("Failed to parse NPC phrase");
						continue;
					}
					phraseList.push_back(text);
				}
			}

			auto* configElement = object->FirstChildElement("config");
			std::vector<std::u16string> keys = {};

			std::vector<LDFBaseData*> config = {};
			if(configElement) {
				for (auto* key = configElement->FirstChildElement("key"); key != nullptr;
					key = key->NextSiblingElement("key")) {
					// Get the config data
					auto* data = key->GetText();
					if (!data) continue;

					LDFBaseData* configData = LDFBaseData::DataFromString(data);
					if (configData->GetKey() == u"useLocationsAsRandomSpawnPoint" && configData->GetValueType() == eLDFType::LDF_TYPE_BOOLEAN){
						useLocationsAsRandomSpawnPoint = static_cast<bool>(configData);
						continue;
					}
					keys.push_back(configData->GetKey());
					config.push_back(configData);
				}
			}
			if (!keys.empty()) config.push_back(new LDFData<std::vector<std::u16string>>(u"syncLDF", keys));

			VanityObject objectData {
				.m_Name = name,
				.m_LOT = std::stoi(lot),
				.m_Equipment = inventory,
				.m_Phrases = phraseList,
				.m_Config = config
			};

			// Get the locations
			auto* locations = object->FirstChildElement("locations");

			if (locations == nullptr) {
				LOG("Failed to parse NPC locations");
				continue;
			}

			for (auto* location = locations->FirstChildElement("location"); location != nullptr;
				location = location->NextSiblingElement("location")) {
				
				// Get the location data
				auto* zoneID = location->Attribute("zone");
				auto* x = location->Attribute("x");
				auto* y = location->Attribute("y");
				auto* z = location->Attribute("z");
				auto* rw = location->Attribute("rw");
				auto* rx = location->Attribute("rx");
				auto* ry = location->Attribute("ry");
				auto* rz = location->Attribute("rz");

				if (zoneID == nullptr || x == nullptr || y == nullptr || z == nullptr || rw == nullptr || rx == nullptr || ry == nullptr
					|| rz == nullptr) {
					LOG("Failed to parse NPC location data");
					continue;
				}

				VanityObjectLocation locationData {
					.m_Position = { std::stof(x), std::stof(y), std::stof(z) },
					.m_Rotation = { std::stof(rw), std::stof(rx), std::stof(ry), std::stof(rz) },
				};

				if (location->Attribute("chance")) {
					locationData.m_Chance = std::stof(location->Attribute("chance"));
				}

				if (location->Attribute("scale")) {
					locationData.m_Scale = std::stof(location->Attribute("scale"));
				}

				const auto& it = objectData.m_Locations.find(std::stoi(zoneID));

				if (it != objectData.m_Locations.end()) {
					it->second.push_back(locationData);
				} else {
					std::vector<VanityObjectLocation> locations;
					locations.push_back(locationData);
					objectData.m_Locations.insert(std::make_pair(std::stoi(zoneID), locations));
				}

				if (!useLocationsAsRandomSpawnPoint) {
					m_Objects.push_back(objectData);
					objectData.m_Locations.clear();
				}
			}

			if (useLocationsAsRandomSpawnPoint) {
				m_Objects.push_back(objectData);
			}
		}
	}
}

VanityObject* VanityUtilities::GetObject(const std::string& name) {
	for (size_t i = 0; i < m_Objects.size(); i++) {
		if (m_Objects[i].m_Name == name) {
			return &m_Objects[i];
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

void NPCTalk(Entity* npc) {
	auto* proximityMonitorComponent = npc->GetComponent<ProximityMonitorComponent>();

	if (!proximityMonitorComponent->GetProximityObjects("talk").empty()) {
		const auto& chats = npc->GetVar<std::vector<std::string>>(u"chats");

		if (chats.empty()) {
			return;
		}

		const auto& selected
			= chats[GeneralUtils::GenerateRandomNumber<int32_t>(0, static_cast<int32_t>(chats.size() - 1))];

		GameMessages::SendNotifyClientZoneObject(
			npc->GetObjectID(), u"sendToclient_bubble", 0, 0, npc->GetObjectID(), selected, UNASSIGNED_SYSTEM_ADDRESS);
	}

	Game::entityManager->SerializeEntity(npc);

	const float nextTime = GeneralUtils::GenerateRandomNumber<float>(15, 60);

	npc->AddCallbackTimer(nextTime, [npc]() { NPCTalk(npc); });
}
