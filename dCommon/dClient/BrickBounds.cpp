#include "BrickBounds.h"

#include "AssetManager.h"
#include "GeneralUtils.h"
#include "Logger.h"
#include "Zip.h"

#include "tinyxml2.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {
	constexpr std::string_view PRIMITIVES = "primitives/";
	constexpr std::string_view XML = ".xml";

	// The design ID of a Primitives/<designID>.xml entry
	std::optional<uint32_t> PrimitiveDesign(const std::string_view name) {
		if (name.size() <= PRIMITIVES.size() + XML.size()) return std::nullopt;
		std::string lower(name);
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (!lower.starts_with(PRIMITIVES) || !lower.ends_with(XML)) return std::nullopt;
		const auto id = std::string_view(lower).substr(PRIMITIVES.size(), lower.size() - PRIMITIVES.size() - XML.size());
		if (id.empty() || !std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isdigit(c); })) return std::nullopt;
		return GeneralUtils::TryParse<uint32_t>(id);
	}
}

namespace {
	std::optional<BrickBounds::Bounds> ReadBounds(const tinyxml2::XMLElement* primitive) {
		const auto* bounding = primitive ? primitive->FirstChildElement("Bounding") : nullptr;
		const auto* aabb = bounding ? bounding->FirstChildElement("AABB") : nullptr;
		if (!aabb) return std::nullopt;

		BrickBounds::Bounds bounds;
		if (aabb->QueryFloatAttribute("minX", &bounds.min.x) != tinyxml2::XML_SUCCESS ||
			aabb->QueryFloatAttribute("minY", &bounds.min.y) != tinyxml2::XML_SUCCESS ||
			aabb->QueryFloatAttribute("minZ", &bounds.min.z) != tinyxml2::XML_SUCCESS ||
			aabb->QueryFloatAttribute("maxX", &bounds.max.x) != tinyxml2::XML_SUCCESS ||
			aabb->QueryFloatAttribute("maxY", &bounds.max.y) != tinyxml2::XML_SUCCESS ||
			aabb->QueryFloatAttribute("maxZ", &bounds.max.z) != tinyxml2::XML_SUCCESS) {
			return std::nullopt;
		}
		return bounds;
	}
}

std::optional<BrickBounds::Bounds> BrickBounds::ParsePrimitive(const std::string_view xml) {
	tinyxml2::XMLDocument doc;
	if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) return std::nullopt;
	return ReadBounds(doc.FirstChildElement("LEGOPrimitive"));
}

BrickBounds::DesignBounds BrickBounds::ParseBrickDb(const std::string_view zip) {
	DesignBounds bounds;
	DesignBounds aliases;
	Zip::ForEachEntry(zip, [](std::string_view name) { return PrimitiveDesign(name).has_value(); },
		[&bounds, &aliases](std::string_view name, std::string_view data) {
			const auto design = PrimitiveDesign(name);
			tinyxml2::XMLDocument doc;
			if (!design || doc.Parse(data.data(), data.size()) != tinyxml2::XML_SUCCESS) return;
			const auto* primitive = doc.FirstChildElement("LEGOPrimitive");
			const auto box = ReadBounds(primitive);
			if (!box) return;
			bounds[*design] = *box;

			// <Annotation aliases="6223;3003"/>: other design IDs of the same brick
			const auto* annotations = primitive->FirstChildElement("Annotations");
			for (const auto* annotation = annotations ? annotations->FirstChildElement("Annotation") : nullptr; annotation; annotation = annotation->NextSiblingElement("Annotation")) {
				const char* list = annotation->Attribute("aliases");
				if (!list) continue;
				for (const auto& alias : GeneralUtils::SplitString(list, ';')) {
					if (const auto id = GeneralUtils::TryParse<uint32_t>(alias)) aliases.emplace(*id, *box);
				}
			}
		});
	// A primitive of its own wins over being another's alias
	for (const auto& [design, box] : aliases) bounds.emplace(design, box);
	return bounds;
}

BrickBounds::DesignBounds BrickBounds::Load(const AssetManager& assets) {
	char* data = nullptr;
	uint32_t length = 0;
	if (!assets.GetFile("brickdb.zip", &data, &length)) {
		LOG("Failed to read brickdb.zip; models are placed by their bricks' origins");
		return {};
	}
	auto bounds = ParseBrickDb(std::string_view(data, length));
	free(data);
	LOG("Read the bounds of %u brick designs", static_cast<uint32_t>(bounds.size()));
	return bounds;
}

const BrickBounds::DesignBounds& BrickBounds::Get(const AssetManager* assets) {
	static const DesignBounds bounds = assets ? Load(*assets) : DesignBounds{};
	return bounds;
}
