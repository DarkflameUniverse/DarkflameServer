#include <sstream>
#include <fstream>

#include "BrickDatabase.h"
#include "Game.h"

std::vector<Brick> BrickDatabase::emptyCache{};
BrickDatabase* BrickDatabase::m_Address = nullptr;

BrickDatabase::BrickDatabase() = default;
BrickDatabase::~BrickDatabase() = default;

std::vector<Brick>& BrickDatabase::GetBricks(const std::string& lxfmlPath) {
	const auto cached = m_Cache.find(lxfmlPath);

	if (cached != m_Cache.end()) {
		return cached->second;
	}

	std::ifstream file(lxfmlPath);
	if (!file.good()) {
		return emptyCache;
	}

	std::stringstream data;
	data << file.rdbuf();
	if (data.str().empty()) {
		return emptyCache;
	}

	auto* doc = new tinyxml2::XMLDocument();
	if (doc->Parse(data.str().c_str(), data.str().size()) != 0) {
		delete doc;
		return emptyCache;
	}

	std::vector<Brick> parts;

	auto* lxfml = doc->FirstChildElement("LXFML");
	auto* bricks = lxfml->FirstChildElement("Bricks");
	std::string searchTerm = "Brick";

	if (!bricks) {
		searchTerm = "Part";
		bricks = lxfml->FirstChildElement("Scene")->FirstChildElement("Model")->FirstChildElement("Group");

		if (!bricks) {
			return emptyCache;
		}
	}

	auto* currentBrick = bricks->FirstChildElement(searchTerm.c_str());
	while (currentBrick != nullptr) {

		auto* part = currentBrick->FirstChildElement("Part");
		if (part == nullptr) part = currentBrick;

		if (part->Attribute("designID") != nullptr) {
			Brick brick{ static_cast<uint32_t>(part->IntAttribute("designID")) };

			// Depends on the file, some don't specify a list but just a single material
			const auto* materialList = part->Attribute("materials");
			const auto* materialID = part->Attribute("materialID");

			if (materialList != nullptr) {
				std::string materialString(materialList);
				const auto materials = GeneralUtils::SplitString(materialString, ',');

				if (!materials.empty()) {
					brick.materialID = std::stoi(materials[0]);
				} else {
					brick.materialID = 0;
				}
			} else if (materialID != nullptr) {
				brick.materialID = std::stoi(materialID);
			} else {
				brick.materialID = 0; // This is bad, makes it so the minigame can't be played
			}

			parts.push_back(brick);
		}

		currentBrick = currentBrick->NextSiblingElement(searchTerm.c_str());
	}

	m_Cache[lxfmlPath] = parts;

	delete doc;

	return m_Cache[lxfmlPath];
}
