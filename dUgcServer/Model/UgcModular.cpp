#include "UgcModular.h"

#include <deque>
#include <set>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "tinyxml2.h"

namespace {
	const tinyxml2::XMLElement* Child(const tinyxml2::XMLElement* element, const char* name) {
		return element ? element->FirstChildElement(name) : nullptr;
	}
}

namespace UgcModular {
	std::optional<BuildInfo> ParseBuild(std::string_view xml) {
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) return std::nullopt;
		const auto* root = doc.FirstChildElement("ModularBuild");
		const auto* topology = Child(root, "topology");
		if (!topology) return std::nullopt;
		BuildInfo build;
		if (const auto* rootPart = Child(topology, "rootPart")) build.rootPart = rootPart->UnsignedAttribute("value");
		if (const auto* count = Child(topology, "numberOfParts")) build.parts = count->UnsignedAttribute("value");
		for (const auto* connection = topology->FirstChildElement("connection"); connection; connection = connection->NextSiblingElement("connection")) {
			const char* location = connection->Attribute("myLocation");
			build.connections.push_back({ connection->UnsignedAttribute("myPartid"), location ? location : "", connection->UnsignedAttribute("connectingPart") });
		}
		if (const auto* rotation = Child(Child(Child(root, "Placement"), "AdditionalModelRotation"), "Rotation")) {
			glm::quat q(rotation->FloatAttribute("w", 1.0f), rotation->FloatAttribute("x"), rotation->FloatAttribute("y"), rotation->FloatAttribute("z"));
			if (glm::length(q) > 0.0f) build.additionalRotation = glm::mat4_cast(glm::normalize(q));
		}
		return build;
	}

	std::map<std::string, glm::vec3> ParseModuleConnections(std::string_view xml) {
		std::map<std::string, glm::vec3> connections;
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) return connections;
		const auto* root = doc.FirstChildElement("ModuleInfo");
		if (!root) return connections;
		for (const auto* connection = root->FirstChildElement("connection"); connection; connection = connection->NextSiblingElement("connection")) {
			const char* name = connection->Attribute("name");
			const auto* translation = connection->FirstChildElement("translation");
			if (!name || !translation) continue;
			connections[name] = glm::vec3(translation->FloatAttribute("x"), translation->FloatAttribute("y"), translation->FloatAttribute("z"));
		}
		return connections;
	}

	std::vector<uint32_t> ParseModuleLots(std::string_view ldf) {
		std::vector<uint32_t> lots;
		size_t start = 0;
		while (start <= ldf.size()) {
			size_t end = ldf.find_first_of("+;,", start);
			if (end == std::string_view::npos) end = ldf.size();
			auto item = ldf.substr(start, end - start);
			if (const auto colon = item.find(':'); colon != std::string_view::npos) item = item.substr(colon + 1);
			uint32_t value = 0;
			bool digits = !item.empty();
			for (const char c : item) {
				if (c < '0' || c > '9') {
					digits = false;
					break;
				}
				value = value * 10 + static_cast<uint32_t>(c - '0');
			}
			if (digits && value != 0) lots.push_back(value);
			start = end + 1;
		}
		return lots;
	}

	UgcModel::Model Assemble(const BuildInfo& build, const std::vector<Module>& modules, std::string& warnings) {
		UgcModel::Model model;
		std::map<uint32_t, const Module*> byPart;
		for (const auto& module : modules) byPart.emplace(module.partCode, &module);

		std::map<uint32_t, glm::mat4> placed;
		std::deque<uint32_t> pending;
		if (byPart.contains(build.rootPart)) {
			placed[build.rootPart] = glm::mat4(1.0f);
			pending.push_back(build.rootPart);
		} else if (!modules.empty()) {
			warnings += "no module for the root part " + std::to_string(build.rootPart) + "; ";
			placed[modules.front().partCode] = glm::mat4(1.0f);
			pending.push_back(modules.front().partCode);
		}

		while (!pending.empty()) {
			const auto part = pending.front();
			pending.pop_front();
			const auto* parent = byPart.at(part);
			for (const auto& connection : build.connections) {
				if (connection.part != part || placed.contains(connection.connectingPart)) continue;
				const auto childIt = byPart.find(connection.connectingPart);
				if (childIt == byPart.end()) continue; // optional parts
				glm::vec3 anchor(0.0f);
				if (const auto node = parent->nif.nodes.find(connection.location); node != parent->nif.nodes.end()) {
					anchor = glm::vec3(node->second.translation[0], node->second.translation[1], node->second.translation[2]);
				} else if (const auto offset = parent->connections.find(connection.location); offset != parent->connections.end()) {
					anchor = offset->second;
				} else {
					warnings += "part " + std::to_string(part) + " has no " + connection.location + "; ";
				}
				const auto& child = *childIt->second;
				if (const auto node = child.nif.nodes.find(connection.location); node != child.nif.nodes.end()) {
					anchor -= glm::vec3(node->second.translation[0], node->second.translation[1], node->second.translation[2]);
				}
				placed[connection.connectingPart] = placed[part] * glm::translate(glm::mat4(1.0f), anchor);
				pending.push_back(connection.connectingPart);
			}
		}

		for (const auto& module : modules) {
			const auto it = placed.find(module.partCode);
			if (it == placed.end()) {
				warnings += "part " + std::to_string(module.partCode) + " isn't connected; ";
				continue;
			}
			auto mesh = UgcModel::FromNif(module.nif);
			mesh.opaque.Transform(it->second);
			mesh.transparent.Transform(it->second);
			model.opaque.Append(mesh.opaque);
			model.transparent.Append(mesh.transparent);
			model.bricks++;
		}
		return model;
	}
}
