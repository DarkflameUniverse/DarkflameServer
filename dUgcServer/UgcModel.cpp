#include "UgcModel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <sstream>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "NifFile.h"
#include "tinyxml2.h"

namespace {
	std::vector<float> ParseFloats(const char* text) {
		std::vector<float> values;
		if (!text) return values;
		std::stringstream stream(text);
		std::string item;
		while (std::getline(stream, item, ',')) {
			try {
				values.push_back(std::stof(item));
			} catch (...) {
				values.push_back(0.0f);
			}
		}
		return values;
	}

	std::vector<uint32_t> ParseMaterials(const char* text) {
		std::vector<uint32_t> values;
		if (!text) return values;
		std::stringstream stream(text);
		std::string item;
		while (std::getline(stream, item, ',')) {
			try {
				values.push_back(static_cast<uint32_t>(std::stoul(item)));
			} catch (...) {
				values.push_back(0);
			}
		}
		// Material 0 means "the same as the part's first"
		for (auto& value : values) {
			if (value == 0 && !values.empty()) value = values[0];
		}
		return values;
	}

	// LXFML 5 bone: a row-major 3x3 rotation followed by a translation
	glm::mat4 BoneMatrix(const std::vector<float>& t) {
		glm::mat4 matrix(1.0f);
		if (t.size() < 12) return matrix;
		matrix[0] = glm::vec4(t[0], t[1], t[2], 0.0f);
		matrix[1] = glm::vec4(t[3], t[4], t[5], 0.0f);
		matrix[2] = glm::vec4(t[6], t[7], t[8], 0.0f);
		matrix[3] = glm::vec4(t[9], t[10], t[11], 1.0f);
		return matrix;
	}

	// LXFML 4: rotate `angle` degrees around (ax, ay, az), then translate
	glm::mat4 AxisAngleMatrix(const tinyxml2::XMLElement* element) {
		const glm::vec3 axis(element->FloatAttribute("ax"), element->FloatAttribute("ay"), element->FloatAttribute("az"));
		const glm::vec3 translation(element->FloatAttribute("tx"), element->FloatAttribute("ty"), element->FloatAttribute("tz"));
		glm::mat4 matrix = glm::translate(glm::mat4(1.0f), translation);
		if (glm::dot(axis, axis) > 0.0f) matrix = glm::rotate(matrix, glm::radians(element->FloatAttribute("angle")), glm::normalize(axis));
		return matrix;
	}

	bool ParseDesign(const tinyxml2::XMLElement* element, uint32_t& design) {
		const char* text = element->Attribute("designID");
		if (!text || !*text) return false;
		for (const char* c = text; *c; c++) {
			if (*c < '0' || *c > '9') return false;
		}
		try {
			design = static_cast<uint32_t>(std::stoul(text));
		} catch (...) {
			return false;
		}
		return true;
	}

	glm::vec4 ToColor(const UgcBricks::Material& material) {
		return glm::vec4(material.r, material.g, material.b, material.a) / 255.0f;
	}
}

namespace UgcModel {
	std::vector<Part> ParseLxfml(std::string_view lxfml, std::string& error) {
		std::vector<Part> parts;
		tinyxml2::XMLDocument doc;
		if (doc.Parse(lxfml.data(), lxfml.size()) != tinyxml2::XML_SUCCESS) {
			error = "the LXFML is not valid XML";
			return parts;
		}
		const auto* root = doc.FirstChildElement("LXFML");
		if (!root) {
			error = "no LXFML element";
			return parts;
		}

		if (const auto* bricks = root->FirstChildElement("Bricks")) {
			for (const auto* brick = bricks->FirstChildElement("Brick"); brick; brick = brick->NextSiblingElement("Brick")) {
				for (const auto* partElement = brick->FirstChildElement("Part"); partElement; partElement = partElement->NextSiblingElement("Part")) {
					const auto* bone = partElement->FirstChildElement("Bone");
					Part part;
					if (!bone || !ParseDesign(partElement, part.designId)) continue;
					const char* materials = partElement->Attribute("materials");
					if (!materials) materials = partElement->Attribute("materialID");
					part.materials = ParseMaterials(materials ? materials : "0");
					part.transform = BoneMatrix(ParseFloats(bone->Attribute("transformation")));
					parts.push_back(std::move(part));
				}
			}
			if (!parts.empty()) return parts;
		}

		// LXFML 4: groups (with their own transforms) nest parts
		std::function<void(const tinyxml2::XMLElement*, const glm::mat4&)> walk = [&](const tinyxml2::XMLElement* element, const glm::mat4& parent) {
			for (const auto* child = element->FirstChildElement(); child; child = child->NextSiblingElement()) {
				const std::string_view name = child->Name();
				if (name != "Group" && name != "Part") continue;
				const auto matrix = parent * AxisAngleMatrix(child);
				if (name == "Group") {
					walk(child, matrix);
					continue;
				}
				Part part;
				if (!ParseDesign(child, part.designId)) continue;
				const char* material = child->Attribute("materialID");
				part.materials = ParseMaterials(material ? material : "0");
				part.transform = matrix;
				parts.push_back(std::move(part));
			}
		};
		if (const auto* scene = root->FirstChildElement("Scene")) {
			for (const auto* model = scene->FirstChildElement("Model"); model; model = model->NextSiblingElement("Model")) walk(model, glm::mat4(1.0f));
		}
		if (parts.empty()) error = "the LXFML has no bricks";
		return parts;
	}

	void Mesh::Append(const Mesh& other) {
		const auto base = static_cast<uint32_t>(positions.size());
		positions.insert(positions.end(), other.positions.begin(), other.positions.end());
		normals.insert(normals.end(), other.normals.begin(), other.normals.end());
		colors.insert(colors.end(), other.colors.begin(), other.colors.end());
		indices.reserve(indices.size() + other.indices.size());
		for (const auto index : other.indices) indices.push_back(base + index);
	}

	void Mesh::Transform(const glm::mat4& transform) {
		const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
		for (auto& position : positions) position = glm::vec3(transform * glm::vec4(position, 1.0f));
		for (auto& normal : normals) {
			const auto n = normalMatrix * normal;
			const auto length = glm::length(n);
			normal = length > 0.0f ? n / length : n;
		}
	}

	bool Model::Bounds(glm::vec3& min, glm::vec3& max) const {
		bool any = false;
		for (const auto* mesh : { &opaque, &transparent }) {
			for (const auto& position : mesh->positions) {
				min = any ? glm::min(min, position) : position;
				max = any ? glm::max(max, position) : position;
				any = true;
			}
		}
		return any;
	}

	Model Build(const std::vector<Part>& parts, UgcBricks::BrickLibrary& library) {
		Model model;
		std::set<uint32_t> missing;
		for (const auto& part : parts) {
			const auto design = library.GetDesign(part.designId);
			if (!design || design->empty()) {
				missing.insert(part.designId);
				continue;
			}
			model.bricks++;
			const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(part.transform)));
			for (size_t index = 0; index < design->size(); index++) {
				const auto& geometry = (*design)[index];
				const auto materialId = index < part.materials.size() ? part.materials[index] : (part.materials.empty() ? 0 : part.materials[0]);
				const auto material = library.GetMaterial(materialId);
				auto& mesh = material.Transparent() ? model.transparent : model.opaque;
				const auto base = static_cast<uint32_t>(mesh.positions.size());
				const auto color = ToColor(material);
				const size_t vertexCount = geometry.positions.size() / 3;
				for (size_t v = 0; v < vertexCount; v++) {
					const glm::vec3 position(geometry.positions[v * 3], geometry.positions[v * 3 + 1], geometry.positions[v * 3 + 2]);
					glm::vec3 normal(geometry.normals[v * 3], geometry.normals[v * 3 + 1], geometry.normals[v * 3 + 2]);
					normal = normalMatrix * normal;
					const auto length = glm::length(normal);
					if (length > 0.0f) normal /= length;
					mesh.positions.push_back(glm::vec3(part.transform * glm::vec4(position, 1.0f)));
					mesh.normals.push_back(normal);
					mesh.colors.push_back(color);
				}
				for (const auto i : geometry.indices) mesh.indices.push_back(base + i);
			}
		}
		model.missingDesigns.assign(missing.begin(), missing.end());
		return model;
	}

	Model FromNif(const NifFile::Model& nif) {
		Model model;
		for (const auto& source : nif.meshes) {
			Mesh mesh;
			const size_t count = source.positions.size() / 3;
			const bool vertexColors = source.material.vertexColorMode == 2 && source.colors.size() == count * 4;
			const glm::vec4 materialColor(source.material.diffuse[0], source.material.diffuse[1], source.material.diffuse[2], source.material.alpha);
			for (size_t v = 0; v < count; v++) {
				mesh.positions.emplace_back(source.positions[v * 3], source.positions[v * 3 + 1], source.positions[v * 3 + 2]);
				if (source.normals.size() == count * 3) mesh.normals.emplace_back(source.normals[v * 3], source.normals[v * 3 + 1], source.normals[v * 3 + 2]);
				glm::vec4 color = materialColor;
				if (vertexColors) color *= glm::vec4(source.colors[v * 4], source.colors[v * 4 + 1], source.colors[v * 4 + 2], source.colors[v * 4 + 3]) / 255.0f;
				mesh.colors.push_back(color);
			}
			mesh.indices.assign(source.indices.begin(), source.indices.end());
			// Normals from the faces when the file has none
			if (mesh.normals.size() != count) {
				mesh.normals.assign(count, glm::vec3(0.0f));
				for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
					const auto& a = mesh.positions[mesh.indices[i]];
					const auto face = glm::cross(mesh.positions[mesh.indices[i + 1]] - a, mesh.positions[mesh.indices[i + 2]] - a);
					for (int k = 0; k < 3; k++) mesh.normals[mesh.indices[i + k]] += face;
				}
				for (auto& normal : mesh.normals) {
					const auto length = glm::length(normal);
					normal = length > 0.0f ? normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
				}
			}
			(source.material.alphaBlend ? model.transparent : model.opaque).Append(mesh);
		}
		return model;
	}

	std::vector<Mesh> Split(const Mesh& mesh, size_t maxVertices, size_t maxTriangles) {
		std::vector<Mesh> pieces;
		if (mesh.positions.size() <= maxVertices && mesh.TriangleCount() <= maxTriangles) {
			if (!mesh.Empty()) pieces.push_back(mesh);
			return pieces;
		}
		Mesh current;
		std::unordered_map<uint32_t, uint32_t> remap;
		const auto flush = [&]() {
			if (!current.Empty()) pieces.push_back(std::move(current));
			current = Mesh{};
			remap.clear();
		};
		for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
			size_t newVertices = 0;
			for (int k = 0; k < 3; k++) newVertices += remap.contains(mesh.indices[i + k]) ? 0 : 1;
			if (current.positions.size() + newVertices > maxVertices || current.TriangleCount() + 1 > maxTriangles) flush();
			for (int k = 0; k < 3; k++) {
				const auto source = mesh.indices[i + k];
				auto [it, added] = remap.try_emplace(source, static_cast<uint32_t>(current.positions.size()));
				if (added) {
					current.positions.push_back(mesh.positions[source]);
					if (source < mesh.normals.size()) current.normals.push_back(mesh.normals[source]);
					if (source < mesh.colors.size()) current.colors.push_back(mesh.colors[source]);
				}
				current.indices.push_back(it->second);
			}
		}
		flush();
		return pieces;
	}
}
