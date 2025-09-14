#include "Lxfml.h"

#include "GeneralUtils.h"
#include "StringifiedEnum.h"
#include "TinyXmlUtils.h"

#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <sstream>

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

// Deep-clone an XMLElement (attributes, text, and child elements) into a target document
static tinyxml2::XMLElement* CloneElementDeep(const tinyxml2::XMLElement* src, tinyxml2::XMLDocument& dstDoc) {
	if (!src) return nullptr;
	auto* dst = dstDoc.NewElement(src->Name());

	// copy attributes
	for (const tinyxml2::XMLAttribute* attr = src->FirstAttribute(); attr; attr = attr->Next()) {
		dst->SetAttribute(attr->Name(), attr->Value());
	}

	// copy children (elements and text)
	for (const tinyxml2::XMLNode* child = src->FirstChild(); child; child = child->NextSibling()) {
		if (const tinyxml2::XMLElement* childElem = child->ToElement()) {
			dst->InsertEndChild(CloneElementDeep(childElem, dstDoc));
		} else if (const tinyxml2::XMLText* txt = child->ToText()) {
			auto* n = dstDoc.NewText(txt->Value());
			dst->InsertEndChild(n);
		} else if (const tinyxml2::XMLComment* c = child->ToComment()) {
			auto* n = dstDoc.NewComment(c->Value());
			dst->InsertEndChild(n);
		}
	}

	return dst;
}

std::vector<Lxfml::Result> Lxfml::Split(const std::string_view data, const NiPoint3& curPosition) {
	std::vector<Result> results;
	tinyxml2::XMLDocument doc;
	const auto err = doc.Parse(data.data());
	if (err != tinyxml2::XML_SUCCESS) {
		LOG("Failed to parse xml %s.", StringifiedEnum::ToString(err).data());
		return results;
	}

	auto* lxfml = doc.FirstChildElement("LXFML");
	if (!lxfml) {
		LOG("Failed to find LXFML element.");
		return results;
	}

	// Build maps: partRef -> Part element, partRef -> Brick element, boneRef -> partRef, brickRef -> Brick element
	std::unordered_map<std::string, tinyxml2::XMLElement*> partRefToPart;
	std::unordered_map<std::string, tinyxml2::XMLElement*> partRefToBrick;
	std::unordered_map<std::string, std::string> boneRefToPartRef;
	std::unordered_map<std::string, tinyxml2::XMLElement*> brickByRef;

	auto* bricksParent = lxfml->FirstChildElement("Bricks");
	if (bricksParent) {
		for (auto* brick = bricksParent->FirstChildElement("Brick"); brick; brick = brick->NextSiblingElement("Brick")) {
			const char* brickRef = brick->Attribute("refID");
			if (brickRef) brickByRef.emplace(std::string(brickRef), brick);
			for (auto* part = brick->FirstChildElement("Part"); part; part = part->NextSiblingElement("Part")) {
				const char* partRef = part->Attribute("refID");
				if (partRef) {
					partRefToPart.emplace(std::string(partRef), part);
					partRefToBrick.emplace(std::string(partRef), brick);
				}
				auto* bone = part->FirstChildElement("Bone");
				if (bone) {
					const char* boneRef = bone->Attribute("refID");
					if (boneRef) boneRefToPartRef.emplace(std::string(boneRef), partRef ? std::string(partRef) : std::string());
				}
			}
		}
	}

	// Collect RigidSystem elements
	std::vector<tinyxml2::XMLElement*> rigidSystems;
	auto* rigidSystemsParent = lxfml->FirstChildElement("RigidSystems");
	if (rigidSystemsParent) {
		for (auto* rs = rigidSystemsParent->FirstChildElement("RigidSystem"); rs; rs = rs->NextSiblingElement("RigidSystem")) {
			rigidSystems.push_back(rs);
		}
	}

	// Collect top-level groups (immediate children of GroupSystem)
	std::vector<tinyxml2::XMLElement*> groupRoots;
	auto* groupSystemsParent = lxfml->FirstChildElement("GroupSystems");
	if (groupSystemsParent) {
		for (auto* gs = groupSystemsParent->FirstChildElement("GroupSystem"); gs; gs = gs->NextSiblingElement("GroupSystem")) {
			for (auto* group = gs->FirstChildElement("Group"); group; group = group->NextSiblingElement("Group")) {
				groupRoots.push_back(group);
			}
		}
	}

	// Track used bricks and rigidsystems
	std::unordered_set<std::string> usedBrickRefs;
	std::unordered_set<tinyxml2::XMLElement*> usedRigidSystems;

	auto splitListAttr = [](const char* attr) {
		std::vector<std::string> out;
		if (!attr) return out;
		std::stringstream ss(attr);
		std::string token;
		while (std::getline(ss, token, ',')) {
			if (!token.empty()) out.push_back(token);
		}
		return out;
	};

	// Helper to create output document from sets of brick refs and rigidsystem pointers
	auto makeOutput = [&](const std::unordered_set<std::string>& bricksToInclude, const std::vector<tinyxml2::XMLElement*>& rigidSystemsToInclude, const std::vector<tinyxml2::XMLElement*>& groupsToInclude = {}) {
		tinyxml2::XMLDocument outDoc;
		outDoc.Parse(Lxfml::base.c_str());
		auto* outRoot = outDoc.FirstChildElement("LXFML");
		auto* outBricks = outRoot->FirstChildElement("Bricks");
		auto* outRigidSystems = outRoot->FirstChildElement("RigidSystems");
		auto* outGroupSystems = outRoot->FirstChildElement("GroupSystems");

		// clone and insert bricks
		for (const auto& bref : bricksToInclude) {
			auto it = brickByRef.find(bref);
			if (it == brickByRef.end()) continue;
			tinyxml2::XMLElement* cloned = CloneElementDeep(it->second, outDoc);
			if (cloned) outBricks->InsertEndChild(cloned);
		}

		// clone and insert rigidsystems
		for (auto* rsPtr : rigidSystemsToInclude) {
			tinyxml2::XMLElement* cloned = CloneElementDeep(rsPtr, outDoc);
			if (cloned) outRigidSystems->InsertEndChild(cloned);
		}

		// clone and insert group(s) if requested
		if (outGroupSystems && !groupsToInclude.empty()) {
			// clear default children
			while (outGroupSystems->FirstChild()) outGroupSystems->DeleteChild(outGroupSystems->FirstChild());
			// create a GroupSystem element and append requested groups
			auto* newGS = outDoc.NewElement("GroupSystem");
			for (auto* gptr : groupsToInclude) {
				tinyxml2::XMLElement* clonedG = CloneElementDeep(gptr, outDoc);
				if (clonedG) newGS->InsertEndChild(clonedG);
			}
			outGroupSystems->InsertEndChild(newGS);
		}

		// Print to string
		tinyxml2::XMLPrinter printer;
		outDoc.Print(&printer);
		// Normalize position and compute center using existing helper
		auto normalized = NormalizePosition(printer.CStr(), curPosition);
		return normalized;
	};

	// 1) Process groups (each top-level Group becomes one output; nested groups are included)
	for (auto* groupRoot : groupRoots) {
		// collect all partRefs in this group's subtree
		std::unordered_set<std::string> partRefs;
		std::function<void(const tinyxml2::XMLElement*)> collectParts = [&](const tinyxml2::XMLElement* g) {
			if (!g) return;
			const char* partAttr = g->Attribute("partRefs");
			if (partAttr) {
				for (auto& tok : splitListAttr(partAttr)) partRefs.insert(tok);
			}
			for (auto* child = g->FirstChildElement("Group"); child; child = child->NextSiblingElement("Group")) collectParts(child);
		};
		collectParts(groupRoot);

		// Build initial sets of bricks and boneRefs
		std::unordered_set<std::string> bricksIncluded;
		std::unordered_set<std::string> boneRefsIncluded;
		for (const auto& pref : partRefs) {
			auto pit = partRefToBrick.find(pref);
			if (pit != partRefToBrick.end()) {
				const char* bref = pit->second->Attribute("refID");
				if (bref) bricksIncluded.insert(std::string(bref));
			}
			auto partIt = partRefToPart.find(pref);
			if (partIt != partRefToPart.end()) {
				auto* bone = partIt->second->FirstChildElement("Bone");
				if (bone) {
					const char* bref = bone->Attribute("refID");
					if (bref) boneRefsIncluded.insert(std::string(bref));
				}
			}
		}

		// Iteratively include any RigidSystems that reference any boneRefsIncluded
		bool changed = true;
		std::vector<tinyxml2::XMLElement*> rigidSystemsToInclude;
		while (changed) {
			changed = false;
			for (auto* rs : rigidSystems) {
				if (usedRigidSystems.find(rs) != usedRigidSystems.end()) continue;
				// parse boneRefs of this rigid system (from its <Rigid> children)
				bool intersects = false;
				std::vector<std::string> rsBoneRefs;
				for (auto* rigid = rs->FirstChildElement("Rigid"); rigid; rigid = rigid->NextSiblingElement("Rigid")) {
					const char* battr = rigid->Attribute("boneRefs");
					if (!battr) continue;
					for (auto& tok : splitListAttr(battr)) {
						rsBoneRefs.push_back(tok);
						if (boneRefsIncluded.find(tok) != boneRefsIncluded.end()) intersects = true;
					}
				}
				if (!intersects) continue;
				// include this rigid system and all boneRefs it references
				usedRigidSystems.insert(rs);
				rigidSystemsToInclude.push_back(rs);
				for (const auto& br : rsBoneRefs) {
					boneRefsIncluded.insert(br);
					auto bpIt = boneRefToPartRef.find(br);
					if (bpIt != boneRefToPartRef.end()) {
						auto partRef = bpIt->second;
						auto pbIt = partRefToBrick.find(partRef);
						if (pbIt != partRefToBrick.end()) {
							const char* bref = pbIt->second->Attribute("refID");
							if (bref && bricksIncluded.insert(std::string(bref)).second) changed = true;
						}
					}
				}
				// also include bricks for any newly discovered parts
			}
		}

		// include bricks from bricksIncluded into used set
		for (const auto& b : bricksIncluded) usedBrickRefs.insert(b);

		// make output doc and push result (include this group's XML)
		std::vector<tinyxml2::XMLElement*> groupsVec{ groupRoot };
		auto normalized = makeOutput(bricksIncluded, rigidSystemsToInclude, groupsVec);
			results.push_back(normalized);
	}

	// 2) Process remaining RigidSystems (each becomes its own file)
	for (auto* rs : rigidSystems) {
		if (usedRigidSystems.find(rs) != usedRigidSystems.end()) continue;
		std::unordered_set<std::string> bricksIncluded;
		// collect boneRefs referenced by this rigid system
		for (auto* rigid = rs->FirstChildElement("Rigid"); rigid; rigid = rigid->NextSiblingElement("Rigid")) {
			const char* battr = rigid->Attribute("boneRefs");
			if (!battr) continue;
			for (auto& tok : splitListAttr(battr)) {
				auto bpIt = boneRefToPartRef.find(tok);
				if (bpIt != boneRefToPartRef.end()) {
					auto partRef = bpIt->second;
					auto pbIt = partRefToBrick.find(partRef);
					if (pbIt != partRefToBrick.end()) {
						const char* bref = pbIt->second->Attribute("refID");
						if (bref) bricksIncluded.insert(std::string(bref));
					}
				}
			}
		}
		// mark used
		for (const auto& b : bricksIncluded) usedBrickRefs.insert(b);
		usedRigidSystems.insert(rs);

		std::vector<tinyxml2::XMLElement*> rsVec{ rs };
		auto normalized = makeOutput(bricksIncluded, rsVec);
		results.push_back(normalized);
	}

	// 3) Any remaining bricks not included become their own files
	for (const auto& [bref, brickPtr] : brickByRef) {
		if (usedBrickRefs.find(bref) != usedBrickRefs.end()) continue;
		std::unordered_set<std::string> bricksIncluded{ bref };
		auto normalized = makeOutput(bricksIncluded, {});
		results.push_back(normalized);
		usedBrickRefs.insert(bref);
	}

	return results;
}
