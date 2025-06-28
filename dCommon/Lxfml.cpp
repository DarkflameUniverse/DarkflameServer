#include "Lxfml.h"

#include "GeneralUtils.h"
#include "StringifiedEnum.h"
#include "TinyXmlUtils.h"

#include <ranges>

Lxfml::Result Lxfml::NormalizePosition(const std::string_view data, const NiPoint3& curPosition) {
	Result toReturn;
	tinyxml2::XMLDocument doc;
	const auto err = doc.Parse(data.data());
	if (err != tinyxml2::XML_SUCCESS) {
		LOG("Failed to parse xml %s.", StringifiedEnum::ToString(err).data());
		return toReturn;
	}

	TinyXmlUtils::DocumentReader reader(doc);
	std::map<std::string/* refID */, std::string> transformations;

	auto lxfml = reader["LXFML"];
	if (!lxfml) {
		LOG("Failed to find LXFML element.");
		return toReturn;
	}

	// First get all the positions of bricks
	for (const auto& brick : lxfml["Bricks"]) {
		const auto* part = brick.FirstChildElement("Part");
		while (part) {
			const auto* bone = part->FirstChildElement("Bone");
			if (bone) {
				auto* transformation = bone->Attribute("transformation");
				if (transformation) {
					auto* refID = bone->Attribute("refID");
					if (refID) transformations[refID] = transformation;
				}
			}
			part = part->NextSiblingElement("Part");
		}
	}

	// These points are well out of bounds for an actual player
	NiPoint3 lowest{ 10'000.0f, 10'000.0f, 10'000.0f };
	NiPoint3 highest{ -10'000.0f, -10'000.0f, -10'000.0f };

	NiPoint3 delta = NiPoint3Constant::ZERO;
	if (curPosition == NiPoint3Constant::ZERO) {
		// Calculate the lowest and highest points on the entire model
		for (const auto& transformation : transformations | std::views::values) {
			auto split = GeneralUtils::SplitString(transformation, ',');
			if (split.size() < 12) {
				LOG("Not enough in the split?");
				continue;
			}

			auto x = GeneralUtils::TryParse<float>(split[9]).value();
			auto y = GeneralUtils::TryParse<float>(split[10]).value();
			auto z = GeneralUtils::TryParse<float>(split[11]).value();
			if (x < lowest.x) lowest.x = x;
			if (y < lowest.y) lowest.y = y;
			if (z < lowest.z) lowest.z = z;

			if (highest.x < x) highest.x = x;
			if (highest.y < y) highest.y = y;
			if (highest.z < z) highest.z = z;
		}

		delta = (highest - lowest) / 2.0f;
	} else {
		lowest = curPosition;
		highest = curPosition;
		delta = NiPoint3Constant::ZERO;
	}

	auto newRootPos = lowest + delta;

	// Need to snap this chosen position to the nearest valid spot
	// on the LEGO grid
	newRootPos.x = GeneralUtils::RountToNearestEven(newRootPos.x, 0.8f);
	newRootPos.z = GeneralUtils::RountToNearestEven(newRootPos.z, 0.8f);

	// Clamp the Y to the lowest point on the model 
	newRootPos.y = lowest.y;

	// Adjust all positions to account for the new origin
	for (auto& transformation : transformations | std::views::values) {
		auto split = GeneralUtils::SplitString(transformation, ',');
		if (split.size() < 12) {
			LOG("Not enough in the split?");
			continue;
		}

		auto x = GeneralUtils::TryParse<float>(split[9]).value() - newRootPos.x + curPosition.x;
		auto y = GeneralUtils::TryParse<float>(split[10]).value() - newRootPos.y + curPosition.y;
		auto z = GeneralUtils::TryParse<float>(split[11]).value() - newRootPos.z + curPosition.z;
		std::stringstream stream;
		for (int i = 0; i < 9; i++) {
			stream << split[i];
			stream << ',';
		}
		stream << x << ',' << y << ',' << z;
		transformation = stream.str();
	}

	// Finally write the new transformation back into the lxfml
	for (auto& brick : lxfml["Bricks"]) {
		auto* part = brick.FirstChildElement("Part");
		while (part) {
			auto* bone = part->FirstChildElement("Bone");
			if (bone) {
				auto* transformation = bone->Attribute("transformation");
				if (transformation) {
					auto* refID = bone->Attribute("refID");
					if (refID) {
						bone->SetAttribute("transformation", transformations[refID].c_str());
					}
				}
			}
			part = part->NextSiblingElement("Part");
		}
	}

	tinyxml2::XMLPrinter printer;
	doc.Print(&printer);

	toReturn.lxfml = printer.CStr();
	toReturn.center = newRootPos;
	return toReturn;
}
