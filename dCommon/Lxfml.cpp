#include "Lxfml.h"

#include "GeneralUtils.h"
#include "StringifiedEnum.h"
#include "TinyXmlUtils.h"

#include <algorithm>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <sstream>
#include <charconv>
#include <optional>

namespace {
	// The base LXFML xml file to use when creating new models.
	std::string g_base = R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
<LXFML versionMajor="5" versionMinor="0">
<Meta>
    <Application name="LEGO Universe" versionMajor="0" versionMinor="0"/>
    <Brand name="LEGOUniverse"/>
    <BrickSet version="457"/>
</Meta>
<Bricks>
</Bricks>
<RigidSystems>
</RigidSystems>
<GroupSystems>
    <GroupSystem>
    </GroupSystem>
</GroupSystems>
</LXFML>)";
}

Lxfml::Contents Lxfml::ReadContents(const std::string_view data) {
	Contents contents;
	if (data.empty()) return contents;

	tinyxml2::XMLDocument doc;
	if (doc.Parse(data.data(), data.size()) != tinyxml2::XML_SUCCESS) return contents;
	const auto* bricks = doc.FirstChildElement("LXFML") ? doc.FirstChildElement("LXFML")->FirstChildElement("Bricks") : nullptr;
	if (!bricks) return contents;

	bool anyBone = false;
	for (const auto* brick = bricks->FirstChildElement("Brick"); brick; brick = brick->NextSiblingElement("Brick")) {
		// designID may carry a suffix ("3001;A")
		const std::string_view design = brick->Attribute("designID") ? brick->Attribute("designID") : "";
		const auto digits = design.substr(0, design.find_first_not_of("0123456789"));
		const auto designId = GeneralUtils::TryParse<uint32_t>(digits);
		if (designId) contents.designIds.push_back(*designId);

		for (const auto* part = brick->FirstChildElement("Part"); part; part = part->NextSiblingElement("Part")) {
			for (const auto* bone = part->FirstChildElement("Bone"); bone; bone = bone->NextSiblingElement("Bone")) {
				const auto* transformation = bone->Attribute("transformation");
				if (!transformation) continue;
				const auto split = GeneralUtils::SplitString(transformation, ',');
				if (split.size() < 12) continue;
				const auto x = GeneralUtils::TryParse<float>(split[9]);
				const auto y = GeneralUtils::TryParse<float>(split[10]);
				const auto z = GeneralUtils::TryParse<float>(split[11]);
				if (!x || !y || !z) continue;
				const NiPoint3 position{ *x, *y, *z };
				if (!anyBone) {
					contents.boxMin = position;
					contents.boxMax = position;
					anyBone = true;
					continue;
				}
				contents.boxMin = NiPoint3(std::min(contents.boxMin.x, position.x), std::min(contents.boxMin.y, position.y), std::min(contents.boxMin.z, position.z));
				contents.boxMax = NiPoint3(std::max(contents.boxMax.x, position.x), std::max(contents.boxMax.y, position.y), std::max(contents.boxMax.z, position.z));
			}
		}
	}
	return contents;
}

namespace {
	// A transformation attribute: 9 rotation values kept as written, then the position
	struct Transformation {
		std::vector<std::string> rotation;
		double x{}, y{}, z{};
	};

	std::optional<Transformation> ParseTransformation(const char* text) {
		if (!text) return std::nullopt;
		auto split = GeneralUtils::SplitString(text, ',');
		if (split.size() < 12) return std::nullopt;
		const auto x = GeneralUtils::TryParse<double>(split[9]);
		const auto y = GeneralUtils::TryParse<double>(split[10]);
		const auto z = GeneralUtils::TryParse<double>(split[11]);
		if (!x || !y || !z) return std::nullopt;
		split.resize(9);
		return Transformation{ std::move(split), *x, *y, *z };
	}

	// The shortest text that reads back as the same float
	std::string FormatNumber(const double value) {
		char buffer[32];
		const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<float>(value));
		return error == std::errc() ? std::string(buffer, end) : std::to_string(value);
	}

	std::string FormatTransformation(const Transformation& transformation) {
		std::string text;
		for (const auto& value : transformation.rotation) text += value + ',';
		return text + FormatNumber(transformation.x) + ',' + FormatNumber(transformation.y) + ',' + FormatNumber(transformation.z);
	}

	// Every element named `name` under `parent`'s children named `childName` (e.g. every Bone of every Part)
	template<typename Visit>
	void ForEachGrandchild(tinyxml2::XMLElement* parent, const char* childName, const char* name, Visit&& visit) {
		if (!parent) return;
		for (auto* child = parent->FirstChildElement(childName); child; child = child->NextSiblingElement(childName)) {
			for (auto* element = child->FirstChildElement(name); element; element = element->NextSiblingElement(name)) visit(element);
		}
	}
}

Lxfml::Result Lxfml::NormalizePosition(const std::string_view data, const NiPoint3& curPosition) {
	Result toReturn;
	if (data.empty()) return toReturn;

	tinyxml2::XMLDocument doc;
	// Use length-based parsing to avoid expensive string copy
	if (doc.Parse(data.data(), data.size()) != tinyxml2::XML_SUCCESS) return toReturn;
	auto* lxfml = doc.FirstChildElement("LXFML");
	if (!lxfml) return toReturn;

	// Every bone of every part (flexible parts have several), and every rigid of every rigid system: both are
	// positioned in the same space, so both move
	std::vector<tinyxml2::XMLElement*> bones;
	if (auto* bricks = lxfml->FirstChildElement("Bricks")) {
		for (auto* brick = bricks->FirstChildElement("Brick"); brick; brick = brick->NextSiblingElement("Brick")) {
			ForEachGrandchild(brick, "Part", "Bone", [&bones](tinyxml2::XMLElement* bone) { bones.push_back(bone); });
		}
	}
	std::vector<tinyxml2::XMLElement*> rigids;
	ForEachGrandchild(lxfml->FirstChildElement("RigidSystems"), "RigidSystem", "Rigid", [&rigids](tinyxml2::XMLElement* rigid) { rigids.push_back(rigid); });

	// The new origin: the middle of the bricks' origins, on the floor of the lowest one. x and z are snapped to the
	// LEGO grid (0.8) so the model's position stays on it; the bricks don't move in the world either way, only the
	// model's pivot does.
	double rootX = curPosition.x, rootY = curPosition.y, rootZ = curPosition.z;
	if (curPosition == NiPoint3Constant::ZERO) {
		bool any = false;
		double minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{};
		for (const auto* bone : bones) {
			const auto transformation = ParseTransformation(bone->Attribute("transformation"));
			if (!transformation) continue;
			const auto& [rotation, x, y, z] = *transformation;
			minX = any ? std::min(minX, x) : x; maxX = any ? std::max(maxX, x) : x;
			minY = any ? std::min(minY, y) : y; maxY = any ? std::max(maxY, y) : y;
			minZ = any ? std::min(minZ, z) : z; maxZ = any ? std::max(maxZ, z) : z;
			any = true;
		}
		// Nothing to place it by: keep the model as it is, at the origin
		if (!any) {
			toReturn.lxfml = std::string(data);
			return toReturn;
		}
		rootX = (minX + maxX) / 2.0;
		rootY = minY;
		rootZ = (minZ + maxZ) / 2.0;
	}
	rootX = GeneralUtils::RountToNearestEven(rootX, 0.8);
	rootZ = GeneralUtils::RountToNearestEven(rootZ, 0.8);

	// Everything moves by the same amount: onto the new origin, then by the given position
	const double offsetX = curPosition.x - rootX, offsetY = curPosition.y - rootY, offsetZ = curPosition.z - rootZ;
	for (auto* elements : { &bones, &rigids }) {
		for (auto* element : *elements) {
			auto transformation = ParseTransformation(element->Attribute("transformation"));
			if (!transformation) continue;
			transformation->x += offsetX;
			transformation->y += offsetY;
			transformation->z += offsetZ;
			element->SetAttribute("transformation", FormatTransformation(*transformation).c_str());
		}
	}

	tinyxml2::XMLPrinter printer;
	doc.Print(&printer);

	toReturn.lxfml = printer.CStr();
	toReturn.center = NiPoint3(static_cast<float>(rootX), static_cast<float>(rootY), static_cast<float>(rootZ));
	return toReturn;
}

static tinyxml2::XMLElement* CloneElementDeep(const tinyxml2::XMLElement* src, tinyxml2::XMLDocument& dstDoc, int maxDepth = 100) {
	if (!src || maxDepth <= 0) return nullptr;
	auto* dst = dstDoc.NewElement(src->Name());

	// copy attributes
	for (const tinyxml2::XMLAttribute* attr = src->FirstAttribute(); attr; attr = attr->Next()) {
		dst->SetAttribute(attr->Name(), attr->Value());
	}

	// copy children (elements and text)
	for (const tinyxml2::XMLNode* child = src->FirstChild(); child; child = child->NextSibling()) {
		if (const tinyxml2::XMLElement* childElem = child->ToElement()) {
			// Recursively clone child elements with decremented depth
			auto* clonedChild = CloneElementDeep(childElem, dstDoc, maxDepth - 1);
			if (clonedChild) dst->InsertEndChild(clonedChild);
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
	
	// Handle empty or invalid input
	if (data.empty()) {
		return results;
	}
	
	// Prevent processing extremely large inputs that could cause hangs
	if (data.size() > 10000000) { // 10MB limit
		return results;
	}
	
	tinyxml2::XMLDocument doc;
	// Use length-based parsing to avoid expensive string copy
	const auto err = doc.Parse(data.data(), data.size());
	if (err != tinyxml2::XML_SUCCESS) {
		return results;
	}

	auto* lxfml = doc.FirstChildElement("LXFML");
	if (!lxfml) {
		return results;
	}

	// Build maps: partRef -> Part element, partRef -> Brick element, boneRef -> partRef, brickRef -> Brick element
	std::unordered_map<std::string, tinyxml2::XMLElement*> partRefToPart;
	std::unordered_map<std::string, tinyxml2::XMLElement*> partRefToBrick;
	std::unordered_map<std::string, std::string> boneRefToPartRef;
	std::unordered_map<std::string, tinyxml2::XMLElement*> brickByRef;
	std::vector<tinyxml2::XMLElement*> brickOrder;

	auto* bricksParent = lxfml->FirstChildElement("Bricks");
	if (bricksParent) {
		for (auto* brick = bricksParent->FirstChildElement("Brick"); brick; brick = brick->NextSiblingElement("Brick")) {
			const char* brickRef = brick->Attribute("refID");
			if (brickRef) brickByRef.emplace(std::string(brickRef), brick);
			brickOrder.push_back(brick);
			for (auto* part = brick->FirstChildElement("Part"); part; part = part->NextSiblingElement("Part")) {
				const char* partRef = part->Attribute("refID");
				if (partRef) {
					partRefToPart.emplace(std::string(partRef), part);
					partRefToBrick.emplace(std::string(partRef), brick);
				}
				// Flexible parts have a bone per section
				for (auto* bone = part->FirstChildElement("Bone"); bone; bone = bone->NextSiblingElement("Bone")) {
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

	// Track used groups to avoid processing them twice
	std::unordered_set<tinyxml2::XMLElement*> usedGroups;

	// Helper to create output document from sets of brick refs and rigidsystem pointers
	auto makeOutput = [&](const std::unordered_set<std::string>& bricksToInclude, const std::vector<tinyxml2::XMLElement*>& rigidSystemsToInclude, const std::vector<tinyxml2::XMLElement*>& groupsToInclude = {}) {
		tinyxml2::XMLDocument outDoc;
		outDoc.Parse(g_base.c_str());
		auto* outRoot = outDoc.FirstChildElement("LXFML");
		auto* outBricks = outRoot->FirstChildElement("Bricks");
		auto* outRigidSystems = outRoot->FirstChildElement("RigidSystems");
		auto* outGroupSystems = outRoot->FirstChildElement("GroupSystems");

		// clone and insert bricks and rigid systems in the order the file has them, so the same model always
		// comes out the same
		for (auto* brick : brickOrder) {
			const char* bref = brick->Attribute("refID");
			// (a refID used twice: the first brick with it, as the maps have it)
			if (!bref || !bricksToInclude.contains(bref) || brickByRef.at(bref) != brick) continue;
			tinyxml2::XMLElement* cloned = CloneElementDeep(brick, outDoc);
			if (cloned) outBricks->InsertEndChild(cloned);
		}
		for (auto* rsPtr : rigidSystems) {
			if (std::find(rigidSystemsToInclude.begin(), rigidSystemsToInclude.end(), rsPtr) == rigidSystemsToInclude.end()) continue;
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

		// Print to string, then normalize position and compute center (the input is at most 10 MB, so each part is too)
		tinyxml2::XMLPrinter printer;
		outDoc.Print(&printer);
		return NormalizePosition(printer.CStr(), curPosition);
	};

	// 1) Process groups (each top-level Group becomes one output; nested groups are included)
	for (auto* groupRoot : groupRoots) {
		// Skip if this group was already processed as part of another group
		if (usedGroups.find(groupRoot) != usedGroups.end()) continue;

		// Helper to collect all partRefs in a group's subtree
		std::function<void(const tinyxml2::XMLElement*, std::unordered_set<std::string>&)> collectParts = [&](const tinyxml2::XMLElement* g, std::unordered_set<std::string>& partRefs) {
			if (!g) return;
			const char* partAttr = g->Attribute("partRefs");
			if (partAttr) {
				for (auto& tok : GeneralUtils::SplitString(partAttr, ',')) partRefs.insert(tok);
			}
			for (auto* child = g->FirstChildElement("Group"); child; child = child->NextSiblingElement("Group")) collectParts(child, partRefs);
		};

		// Collect all groups that need to be merged into this output
		std::vector<tinyxml2::XMLElement*> groupsToInclude{ groupRoot };
		usedGroups.insert(groupRoot);

		// Build initial sets of bricks and boneRefs from the starting group
		std::unordered_set<std::string> partRefs;
		collectParts(groupRoot, partRefs);

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
				for (auto* bone = partIt->second->FirstChildElement("Bone"); bone; bone = bone->NextSiblingElement("Bone")) {
					const char* bref = bone->Attribute("refID");
					if (bref) boneRefsIncluded.insert(std::string(bref));
				}
			}
		}

		// Iteratively include any RigidSystems that reference any boneRefsIncluded
		// and check if those rigid systems' bricks span other groups
		bool changed = true;
		std::vector<tinyxml2::XMLElement*> rigidSystemsToInclude;
		int maxIterations = 1000; // Safety limit to prevent infinite loops
		int iteration = 0;
		while (changed && iteration < maxIterations) {
			changed = false;
			iteration++;

			// First, expand rigid systems based on current boneRefsIncluded
			for (auto* rs : rigidSystems) {
				if (usedRigidSystems.find(rs) != usedRigidSystems.end()) continue;
				// parse boneRefs of this rigid system (from its <Rigid> children)
				bool intersects = false;
				std::vector<std::string> rsBoneRefs;
				for (auto* rigid = rs->FirstChildElement("Rigid"); rigid; rigid = rigid->NextSiblingElement("Rigid")) {
					const char* battr = rigid->Attribute("boneRefs");
					if (!battr) continue;
					for (auto& tok : GeneralUtils::SplitString(battr, ',')) {
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
			}

			// Second, check if the newly included bricks span any other groups
			// If so, merge those groups into the current output
			for (auto* otherGroup : groupRoots) {
				if (usedGroups.find(otherGroup) != usedGroups.end()) continue;

				// Collect partRefs from this other group
				std::unordered_set<std::string> otherPartRefs;
				collectParts(otherGroup, otherPartRefs);

				// Check if any of these partRefs correspond to bricks we've already included
				bool spansOtherGroup = false;
				for (const auto& pref : otherPartRefs) {
					auto pit = partRefToBrick.find(pref);
					if (pit != partRefToBrick.end()) {
						const char* bref = pit->second->Attribute("refID");
						if (bref && bricksIncluded.find(std::string(bref)) != bricksIncluded.end()) {
							spansOtherGroup = true;
							break;
						}
					}
				}

				if (spansOtherGroup) {
					// Merge this group into the current output
					usedGroups.insert(otherGroup);
					groupsToInclude.push_back(otherGroup);
					changed = true;

					// Add all partRefs, boneRefs, and bricks from this group
					for (const auto& pref : otherPartRefs) {
						auto pit = partRefToBrick.find(pref);
						if (pit != partRefToBrick.end()) {
							const char* bref = pit->second->Attribute("refID");
							if (bref) bricksIncluded.insert(std::string(bref));
						}
						auto partIt = partRefToPart.find(pref);
						if (partIt != partRefToPart.end()) {
							for (auto* bone = partIt->second->FirstChildElement("Bone"); bone; bone = bone->NextSiblingElement("Bone")) {
								const char* bref = bone->Attribute("refID");
								if (bref) boneRefsIncluded.insert(std::string(bref));
							}
						}
					}
				}
			}
		}
		
		// (Every pass that goes on adds a rigid system or group, so the limit is never reached; hitting it anyway still
		// outputs what was collected, and the rest of the file comes out as further models below.)
		// include bricks from bricksIncluded into used set
		for (const auto& b : bricksIncluded) usedBrickRefs.insert(b);

		// make output doc and push result (include all merged groups' XML)
		auto normalized = makeOutput(bricksIncluded, rigidSystemsToInclude, groupsToInclude);
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
			for (auto& tok : GeneralUtils::SplitString(battr, ',')) {
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
	for (auto* brick : brickOrder) {
		const char* brefAttr = brick->Attribute("refID");
		if (!brefAttr) continue;
		const std::string bref(brefAttr);
		if (usedBrickRefs.find(bref) != usedBrickRefs.end()) continue;
		std::unordered_set<std::string> bricksIncluded{ bref };
		auto normalized = makeOutput(bricksIncluded, {});
		results.push_back(normalized);
		usedBrickRefs.insert(bref);
	}

	return results;
}
