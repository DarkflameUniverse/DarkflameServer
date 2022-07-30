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
#include "dLogger.h"

#include <fstream>

std::vector<VanityNPC> VanityUtilities::m_NPCs = {};
std::vector<VanityParty> VanityUtilities::m_Parties = {};
std::vector<std::string> VanityUtilities::m_PartyPhrases = {};

void VanityUtilities::SpawnVanity() {
	if (Game::config->GetValue("disable_vanity") == "1") {
		return;
	}

	const uint32_t zoneID = Game::server->GetZoneID();

	ParseXML("./vanity/NPC.xml");

	// Loop through all parties
	for (const auto& party : m_Parties) {
		const auto chance = party.m_Chance;
		const auto zone = party.m_Zone;

		if (zone != Game::server->GetZoneID()) {
			continue;
		}

		float rate = GeneralUtils::GenerateRandomNumber<float>(0, 1);
		if (chance < rate) {
			continue;
		}

		// Copy m_NPCs into a new vector
		std::vector<VanityNPC> npcList = m_NPCs;
		std::vector<uint32_t> taken = {};

		Game::logger->Log("VanityUtilities", "Spawning party with %i locations", party.m_Locations.size());

		// Loop through all locations
		for (const auto& location : party.m_Locations) {
			rate = GeneralUtils::GenerateRandomNumber<float>(0, 1);
			if (0.75f < rate) {
				continue;
			}

			// Get a random NPC
			auto npcIndex = GeneralUtils::GenerateRandomNumber<uint32_t>(0, npcList.size() - 1);

			while (std::find(taken.begin(), taken.end(), npcIndex) != taken.end()) {
				npcIndex = GeneralUtils::GenerateRandomNumber<uint32_t>(0, npcList.size() - 1);
			}

			const auto& npc = npcList[npcIndex];

			taken.push_back(npcIndex);

			// Spawn the NPC
			std::vector<LDFBaseData*> data = { new LDFData<std::vector<std::u16string>>(
											   u"syncLDF", { u"custom_script_client" }),
			new LDFData<std::u16string>(u"custom_script_client", u"scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua") };

			// Spawn the NPC
			auto* npcEntity = SpawnNPC(npc.m_LOT, npc.m_Name, location.m_Position, location.m_Rotation, npc.m_Equipment, data);

			npcEntity->SetVar<std::vector<std::string>>(u"chats", m_PartyPhrases);

			SetupNPCTalk(npcEntity);
		}

		return;
	}

	// Loop through all NPCs
	for (const auto& pair : m_NPCs) {
		if (pair.m_Locations.find(Game::server->GetZoneID()) == pair.m_Locations.end())
			continue;

		const std::vector<VanityNPCLocation>& locations = pair.m_Locations.at(Game::server->GetZoneID());

		// Pick a random location
		const auto& location = locations[GeneralUtils::GenerateRandomNumber<int>(
			static_cast<size_t>(0), static_cast<size_t>(locations.size() - 1))];

		float rate = GeneralUtils::GenerateRandomNumber<float>(0, 1);
		if (location.m_Chance < rate) {
			continue;
		}

		std::vector<LDFBaseData*> data = { new LDFData<std::vector<std::u16string>>(
											   u"syncLDF", { u"custom_script_client" }),
			new LDFData<std::u16string>(u"custom_script_client", u"scripts\\ai\\SPEC\\MISSION_MINIGAME_CLIENT.lua") };

		// Spawn the NPC
		auto* npc = SpawnNPC(pair.m_LOT, pair.m_Name, location.m_Position, location.m_Rotation, pair.m_Equipment, data);

		npc->SetVar<std::vector<std::string>>(u"chats", pair.m_Phrases);

		auto* scriptComponent = npc->GetComponent<ScriptComponent>();

		if (scriptComponent != nullptr) {
			scriptComponent->SetScript(pair.m_Script);
			scriptComponent->SetSerialized(false);

			for (const auto& pair : pair.m_Flags) {
				npc->SetVar<bool>(GeneralUtils::ASCIIToUTF16(pair.first), pair.second);
			}
		}

		SetupNPCTalk(npc);
	}

	if (zoneID == 1200) {
		{
			EntityInfo info;
			info.lot = 8139;
			info.pos = { 259.5f, 246.4f, -705.2f };
			info.rot = { 0.0f, 0.0f, 1.0f, 0.0f };
			info.spawnerID = EntityManager::Instance()->GetZoneControlEntity()->GetObjectID();

			info.settings = { new LDFData<bool>(u"hasCustomText", true),
				new LDFData<std::string>(u"customText", ParseMarkdown("./vanity/TESTAMENT.md")) };

			auto* entity = EntityManager::Instance()->CreateEntity(info);

			EntityManager::Instance()->ConstructEntity(entity);
		}
	}
}

Entity* VanityUtilities::SpawnNPC(LOT lot, const std::string& name, const NiPoint3& position,
	const NiQuaternion& rotation, const std::vector<LOT>& inventory, const std::vector<LDFBaseData*>& ldf) {
	EntityInfo info;
	info.lot = lot;
	info.pos = position;
	info.rot = rotation;
	info.spawnerID = EntityManager::Instance()->GetZoneControlEntity()->GetObjectID();
	info.settings = ldf;

	auto* entity = EntityManager::Instance()->CreateEntity(info);
	entity->SetVar(u"npcName", name);

	auto* inventoryComponent = entity->GetComponent<InventoryComponent>();

	if (inventoryComponent != nullptr) {
		inventoryComponent->SetNPCItems(inventory);
	}

	auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();

	if (destroyableComponent != nullptr) {
		destroyableComponent->SetIsGMImmune(true);
		destroyableComponent->SetMaxHealth(0);
		destroyableComponent->SetHealth(0);
	}

	EntityManager::Instance()->ConstructEntity(entity);

	return entity;
}

void VanityUtilities::ParseXML(const std::string& file) {
	// Read the entire file
	std::ifstream xmlFile(file);
	std::string xml((std::istreambuf_iterator<char>(xmlFile)), std::istreambuf_iterator<char>());

	// Parse the XML
	tinyxml2::XMLDocument doc;
	doc.Parse(xml.c_str(), xml.size());

	// Read the NPCs
	auto* npcs = doc.FirstChildElement("npcs");

	if (npcs == nullptr) {
		Game::logger->Log("VanityUtilities", "Failed to parse NPCs");
		return;
	}

	for (auto* party = npcs->FirstChildElement("party"); party != nullptr; party = party->NextSiblingElement("party")) {
		// Get 'zone' as uint32_t and 'chance' as float
		uint32_t zone = 0;
		float chance = 0.0f;

		if (party->Attribute("zone") != nullptr) {
			zone = std::stoul(party->Attribute("zone"));
		}

		if (party->Attribute("chance") != nullptr) {
			chance = std::stof(party->Attribute("chance"));
		}

		VanityParty partyInfo;
		partyInfo.m_Zone = zone;
		partyInfo.m_Chance = chance;

		auto* locations = party->FirstChildElement("locations");

		if (locations == nullptr) {
			Game::logger->Log("VanityUtilities", "Failed to parse party locations");
			continue;
		}

		for (auto* location = locations->FirstChildElement("location"); location != nullptr;
			location = location->NextSiblingElement("location")) {
			// Get the location data
			auto* x = location->Attribute("x");
			auto* y = location->Attribute("y");
			auto* z = location->Attribute("z");
			auto* rw = location->Attribute("rw");
			auto* rx = location->Attribute("rx");
			auto* ry = location->Attribute("ry");
			auto* rz = location->Attribute("rz");

			if (x == nullptr || y == nullptr || z == nullptr || rw == nullptr || rx == nullptr || ry == nullptr
				|| rz == nullptr) {
				Game::logger->Log("VanityUtilities", "Failed to parse party location data");
				continue;
			}

			VanityNPCLocation locationData;
			locationData.m_Position = { std::stof(x), std::stof(y), std::stof(z) };
			locationData.m_Rotation = { std::stof(rw), std::stof(rx), std::stof(ry), std::stof(rz) };
			locationData.m_Chance = 1.0f;

			partyInfo.m_Locations.push_back(locationData);
		}

		m_Parties.push_back(partyInfo);
	}

	auto* partyPhrases = npcs->FirstChildElement("partyphrases");

	if (partyPhrases == nullptr) {
		Game::logger->Log("VanityUtilities", "Failed to parse party phrases");
		return;
	}

	for (auto* phrase = partyPhrases->FirstChildElement("phrase"); phrase != nullptr;
		phrase = phrase->NextSiblingElement("phrase")) {
		// Get the phrase
		auto* text = phrase->GetText();

		if (text == nullptr) {
			Game::logger->Log("VanityUtilities", "Failed to parse party phrase");
			continue;
		}

		m_PartyPhrases.push_back(text);
	}

	for (auto* npc = npcs->FirstChildElement("npc"); npc != nullptr; npc = npc->NextSiblingElement("npc")) {
		// Get the NPC name
		auto* name = npc->Attribute("name");

		if (name == nullptr) {
			Game::logger->Log("VanityUtilities", "Failed to parse NPC name");
			continue;
		}

		// Get the NPC lot
		auto* lot = npc->Attribute("lot");

		if (lot == nullptr) {
			Game::logger->Log("VanityUtilities", "Failed to parse NPC lot");
			continue;
		}

		// Get the equipment
		auto* equipment = npc->FirstChildElement("equipment");

		if (equipment == nullptr) {
			Game::logger->Log("VanityUtilities", "Failed to parse NPC equipment");
			continue;
		}

		auto* text = equipment->GetText();

		std::vector<LOT> inventory;

		if (text != nullptr) {
			std::string equipmentString(text);

			std::vector<std::string> splitEquipment = GeneralUtils::SplitString(equipmentString, ',');

			for (auto& item : splitEquipment) {
				inventory.push_back(std::stoi(item));
			}
		}

		// Get the phrases
		auto* phrases = npc->FirstChildElement("phrases");

		if (phrases == nullptr) {
			Game::logger->Log("VanityUtilities", "Failed to parse NPC phrases");
			continue;
		}

		std::vector<std::string> phraseList;

		for (auto* phrase = phrases->FirstChildElement("phrase"); phrase != nullptr;
			phrase = phrase->NextSiblingElement("phrase")) {
			// Get the phrase
			auto* text = phrase->GetText();

			if (text == nullptr) {
				Game::logger->Log("VanityUtilities", "Failed to parse NPC phrase");
				continue;
			}

			phraseList.push_back(text);
		}

		// Get the script
		auto* scriptElement = npc->FirstChildElement("script");

		std::string scriptName;

		if (scriptElement != nullptr) {
			auto* scriptNameAttribute = scriptElement->Attribute("name");

			if (scriptNameAttribute == nullptr) {
				Game::logger->Log("VanityUtilities", "Failed to parse NPC script name");
				continue;
			}

			scriptName = scriptNameAttribute;
		}

		VanityNPC npcData;
		npcData.m_Name = name;
		npcData.m_LOT = std::stoi(lot);
		npcData.m_Equipment = inventory;
		npcData.m_Phrases = phraseList;
		npcData.m_Script = scriptName;

		// Get flags
		auto* flags = npc->FirstChildElement("flags");

		if (flags != nullptr) {
			for (auto* flag = flags->FirstChildElement("flag"); flag != nullptr;
				flag = flag->NextSiblingElement("flag")) {
				// Get the flag name
				auto* name = flag->Attribute("name");

				if (name == nullptr) {
					Game::logger->Log("VanityUtilities", "Failed to parse NPC flag name");
					continue;
				}

				// Get the flag value
				auto* value = flag->Attribute("value");

				if (value == nullptr) {
					Game::logger->Log("VanityUtilities", "Failed to parse NPC flag value");
					continue;
				}

				npcData.m_Flags[name] = std::stoi(value);
			}
		}

		// Get the zones
		for (auto* zone = npc->FirstChildElement("zone"); zone != nullptr; zone = zone->NextSiblingElement("zone")) {
			// Get the zone ID
			auto* zoneID = zone->Attribute("id");

			if (zoneID == nullptr) {
				Game::logger->Log("VanityUtilities", "Failed to parse NPC zone ID");
				continue;
			}

			// Get the locations
			auto* locations = zone->FirstChildElement("locations");

			if (locations == nullptr) {
				Game::logger->Log("VanityUtilities", "Failed to parse NPC locations");
				continue;
			}

			for (auto* location = locations->FirstChildElement("location"); location != nullptr;
				location = location->NextSiblingElement("location")) {
				// Get the location data
				auto* x = location->Attribute("x");
				auto* y = location->Attribute("y");
				auto* z = location->Attribute("z");
				auto* rw = location->Attribute("rw");
				auto* rx = location->Attribute("rx");
				auto* ry = location->Attribute("ry");
				auto* rz = location->Attribute("rz");

				if (x == nullptr || y == nullptr || z == nullptr || rw == nullptr || rx == nullptr || ry == nullptr
					|| rz == nullptr) {
					Game::logger->Log("VanityUtilities", "Failed to parse NPC location data");
					continue;
				}

				VanityNPCLocation locationData;
				locationData.m_Position = { std::stof(x), std::stof(y), std::stof(z) };
				locationData.m_Rotation = { std::stof(rw), std::stof(rx), std::stof(ry), std::stof(rz) };
				locationData.m_Chance = 1.0f;

				if (location->Attribute("chance") != nullptr) {
					locationData.m_Chance = std::stof(location->Attribute("chance"));
				}

				const auto& it = npcData.m_Locations.find(std::stoi(zoneID));

				if (it != npcData.m_Locations.end()) {
					it->second.push_back(locationData);
				} else {
					std::vector<VanityNPCLocation> locations;
					locations.push_back(locationData);
					npcData.m_Locations.insert(std::make_pair(std::stoi(zoneID), locations));
				}
			}
		}

		m_NPCs.push_back(npcData);
	}
}

VanityNPC* VanityUtilities::GetNPC(const std::string& name) {
	for (size_t i = 0; i < m_NPCs.size(); i++) {
		if (m_NPCs[i].m_Name == name) {
			return &m_NPCs[i];
		}
	}

	return nullptr;
}

std::string VanityUtilities::ParseMarkdown(const std::string& file) {
	// This function will read the file and return the content formatted as ASCII text.

	// Read the file into a string
	std::ifstream t(file);

	// If the file does not exist, return an empty string.
	if (!t.good()) {
		return "";
	}

	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string fileContents = buffer.str();

	// Loop through all lines in the file.
	// Replace all instances of the markdown syntax with the corresponding HTML.
	// Only care about headers
	std::stringstream output;
	std::string line;
	std::stringstream ss;
	ss << fileContents;
	while (std::getline(ss, line)) {

#define TOSTRING(x) #x
#define STRINGIFY(x) TOSTRING(x)

		// Replace "__TIMESTAMP__" with the __TIMESTAMP__
		GeneralUtils::ReplaceInString(line, "__TIMESTAMP__", __TIMESTAMP__);
		// Replace "__VERSION__" wit'h the PROJECT_VERSION
		GeneralUtils::ReplaceInString(line, "__VERSION__", STRINGIFY(PROJECT_VERSION));
		// Replace "__SOURCE__" with SOURCE
		GeneralUtils::ReplaceInString(line, "__SOURCE__", Game::config->GetValue("source"));
		// Replace "__LICENSE__" with LICENSE
		GeneralUtils::ReplaceInString(line, "__LICENSE__", STRINGIFY(LICENSE));

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

void VanityUtilities::SetupNPCTalk(Entity* npc) {
	npc->AddCallbackTimer(15.0f, [npc]() { NPCTalk(npc); });

	npc->SetProximityRadius(20.0f, "talk");
}

void VanityUtilities::NPCTalk(Entity* npc) {
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

	EntityManager::Instance()->SerializeEntity(npc);

	const float nextTime = GeneralUtils::GenerateRandomNumber<float>(15, 60);

	npc->AddCallbackTimer(nextTime, [npc]() { NPCTalk(npc); });
}
