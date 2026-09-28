#include "UgcModel.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <sstream>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "NifFile.h"
#include "UgcPalette.h"
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

	bool HasNoBricks(std::string_view lxfml) {
		tinyxml2::XMLDocument doc;
		if (doc.Parse(lxfml.data(), lxfml.size()) != tinyxml2::XML_SUCCESS || !doc.FirstChildElement("LXFML")) return false;
		std::string error;
		return ParseLxfml(lxfml, error).empty() && lxfml.find("<Part") == std::string_view::npos;
	}

	void Mesh::Append(const Mesh& other) {
		const auto base = static_cast<uint32_t>(positions.size());
		positions.insert(positions.end(), other.positions.begin(), other.positions.end());
		normals.insert(normals.end(), other.normals.begin(), other.normals.end());
		colors.insert(colors.end(), other.colors.begin(), other.colors.end());
		if (!glow.empty() || !other.glow.empty()) {
			glow.resize(base, glm::vec3(0.0f));
			if (other.glow.empty()) glow.resize(positions.size(), glm::vec3(0.0f));
			else glow.insert(glow.end(), other.glow.begin(), other.glow.end());
		}
		if (!looks.empty() || !other.looks.empty()) {
			looks.resize(base, eLook::PLASTIC);
			if (other.looks.empty()) looks.resize(positions.size(), eLook::PLASTIC);
			else looks.insert(looks.end(), other.looks.begin(), other.looks.end());
		}
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

	eLook LookOf(uint32_t id, const UgcBricks::Material& material, const LookRules& rules) {
		if (rules.paletteGlow && UgcPalette::Glow(id)) return eLook::GLOW;
		if (rules.paletteMetallic && UgcPalette::IsMetallic(id)) return eLook::METAL;
		const auto type = rules.materialTypes.find(material.type);
		return type != rules.materialTypes.end() ? type->second : eLook::PLASTIC;
	}

	std::optional<std::array<Mesh, LOOK_COUNT>> SplitLooks(const Mesh& mesh, const std::array<bool, LOOK_COUNT>& separate) {
		if (mesh.looks.size() != mesh.positions.size()) return std::nullopt;
		const auto lookOf = [&](size_t triangle) {
			const auto look = mesh.looks[mesh.indices[triangle * 3]];
			return separate[static_cast<size_t>(look)] ? look : eLook::PLASTIC;
		};
		bool any = false;
		for (size_t t = 0; t < mesh.TriangleCount() && !any; t++) any = lookOf(t) != eLook::PLASTIC;
		if (!any) return std::nullopt;
		std::array<Mesh, LOOK_COUNT> out;
		for (size_t look = 0; look < LOOK_COUNT; look++) {
			std::vector<bool> keep(mesh.TriangleCount());
			bool some = false;
			for (size_t t = 0; t < keep.size(); t++) some = (keep[t] = static_cast<size_t>(lookOf(t)) == look) || some;
			if (!some) continue;
			out[look] = mesh;
			KeepTriangles(out[look], keep);
		}
		return out;
	}

	Model Build(const std::vector<Part>& parts, UgcBricks::BrickLibrary& library, const BuildOptions& options) {
		Model model;
		std::set<uint32_t> missing;
		const bool luToolbox = options.palette == ePalette::LU_TOOLBOX;
		bool anyGlow = false, anyLook = false;
		for (uint32_t brick = 0; brick < parts.size(); brick++) {
			const auto& part = parts[brick];
			const auto design = library.GetDesign(part.designId, options.lod);
			if (!design || design->empty()) {
				missing.insert(part.designId);
				continue;
			}
			model.bricks++;
			const auto materialOf = [&part, &library](size_t index) {
				auto id = index < part.materials.size() ? part.materials[index] : (part.materials.empty() ? 0 : part.materials[0]);
				// Unknown colors are black in LU Toolbox (its name included, so black's variation too). A color LU
				// Toolbox doesn't know but the client's Materials.xml has (one added to the brick database) keeps its id
				// and takes its color from there.
				if (id == 0 || (!UgcPalette::Linear(id) && !library.HasMaterial(id))) id = UgcPalette::FALLBACK_ID;
				return id;
			};
			// Whether LU Toolbox's own palette colors a material (else it's one only the client's Materials.xml has)
			const auto inToolbox = [](uint32_t id) { return UgcPalette::Linear(id).has_value(); };
			// A brick is transparent only when all of its materials are (LU Toolbox's IS_TRANSPARENT)
			bool transparent = true;
			for (size_t index = 0; index < design->size(); index++) {
				const auto id = index < part.materials.size() ? part.materials[index] : (part.materials.empty() ? 0 : part.materials[0]);
				const auto toolboxId = materialOf(index);
				transparent = transparent && (luToolbox && inToolbox(toolboxId) ? UgcPalette::IsTransparent(toolboxId) : library.GetMaterial(luToolbox ? toolboxId : id).Transparent());
			}
			auto& mesh = transparent ? model.transparent : model.opaque;
			if (transparent) model.transparentBricks.push_back(mesh.indices.size());
			const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(part.transform)));
			for (size_t index = 0; index < design->size(); index++) {
				const auto& geometry = (*design)[index];
				glm::vec3 linear{};
				float alpha = 1.0f;
				glm::vec3 glow(0.0f);
				uint32_t colorId{};
				if (luToolbox && !inToolbox(materialOf(index))) {
					// A color only the client's Materials.xml has
					colorId = materialOf(index);
					const auto material = library.GetMaterial(colorId);
					linear = UgcPalette::SrgbToLinear(glm::vec3(material.r, material.g, material.b) / 255.0f);
					if (transparent) alpha = material.a / 255.0f;
				} else if (luToolbox) {
					colorId = materialOf(index);
					linear = *UgcPalette::Linear(colorId, options.icon);
					if (transparent) alpha = std::clamp(options.transparentOpacity / 100.0f, 0.0f, 1.0f);
					if (!transparent) {
						if (const auto g = UgcPalette::Glow(colorId)) glow = *g;
					}
				} else {
					colorId = index < part.materials.size() ? part.materials[index] : (part.materials.empty() ? 0 : part.materials[0]);
					const auto material = library.GetMaterial(colorId);
					linear = UgcPalette::SrgbToLinear(glm::vec3(material.r, material.g, material.b) / 255.0f);
					if (transparent) alpha = material.a / 255.0f;
				}
				if (options.colorVariation > 0.0f) {
					const float variation = options.colorVariation * (luToolbox ? UgcPalette::VariationScale(colorId) : 1.0f);
					linear = UgcPalette::ApplyVariation(linear, variation, UgcPalette::BrickRandom(options.seed, brick, colorId));
				}
				const glm::vec4 color(UgcPalette::LinearToSrgb(linear), alpha);
				anyGlow = anyGlow || glow != glm::vec3(0.0f);
				const auto look = transparent ? eLook::PLASTIC : LookOf(colorId, library.GetMaterial(colorId), options.looks);
				anyLook = anyLook || look != eLook::PLASTIC;
				const auto base = static_cast<uint32_t>(mesh.positions.size());
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
					if (&mesh == &model.opaque) {
						model.opaque.glow.push_back(glow);
						model.opaque.looks.push_back(look);
					}
				}
				for (const auto i : geometry.indices) mesh.indices.push_back(base + i);
			}
		}
		if (!anyGlow) model.opaque.glow.clear();
		else model.opaque.glow.resize(model.opaque.positions.size(), glm::vec3(0.0f));
		if (!anyLook) model.opaque.looks.clear();
		model.missingDesigns.assign(missing.begin(), missing.end());
		return model;
	}

	std::vector<std::pair<float, float>> LodRanges(const std::vector<uint32_t>& used, const LodDistances& d) {
		// LU Toolbox's setup_lod_data ("DYNAMIC LOD HELL"), by the set of levels there are
		const std::set<uint32_t> set(used.begin(), used.end());
		const auto is = [&set](std::initializer_list<uint32_t> levels) { return set == std::set<uint32_t>(levels); };
		std::vector<std::pair<float, float>> ranges;
		for (const auto level : used) {
			std::pair<float, float> range{ 0.0f, 0.0f };
			if (set.size() == 1) {
				range = { d.lod0, d.cull };
			} else if (level == 0) {
				range.first = d.lod0;
				if (is({ 0, 2 }) || is({ 0, 2, 3 })) range.second = d.lod2;
				else if (is({ 0, 3 })) range.second = d.lod3;
				else range.second = d.lod1;
			} else if (level == 1) {
				if (is({ 0, 1 })) range = { d.lod1, d.cull };
				else if (is({ 1, 2 }) || is({ 1, 2, 3 })) range = { d.lod0, d.lod2 };
				else if (is({ 0, 1, 3 })) range = { d.lod1, d.lod3 };
				else if (is({ 1, 3 })) range = { d.lod0, d.lod3 };
				else if (is({ 0, 1, 2 }) || is({ 0, 1, 2, 3 })) range = { d.lod1, d.lod2 };
			} else if (level == 2) {
				if (is({ 0, 2 }) || is({ 1, 2 }) || is({ 0, 1, 2 })) range = { d.lod2, d.cull };
				else if (is({ 2, 3 })) range = { d.lod0, d.lod3 };
				else if (is({ 0, 2, 3 }) || is({ 1, 2, 3 }) || is({ 0, 1, 2, 3 })) range = { d.lod2, d.lod3 };
			} else if (level == 3) {
				range = { d.lod3, d.cull };
			}
			ranges.push_back(range);
		}
		return ranges;
	}

	Model FromNif(const NifFile::Model& nif, const std::map<int32_t, eLook>& tagLooks) {
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
			// Blending only shows where something is see-through (the game's brick models blend every shape)
			bool seeThrough = source.material.alphaBlend && source.material.alpha < 0.99f;
			for (size_t v = 0; source.material.alphaBlend && !seeThrough && v < mesh.colors.size(); v++) seeThrough = mesh.colors[v].a < 0.99f;
			if (const auto look = tagLooks.find(source.material.shaderTag); !seeThrough && look != tagLooks.end() && look->second != eLook::PLASTIC) {
				mesh.looks.assign(mesh.positions.size(), look->second);
			}
			(seeThrough ? model.transparent : model.opaque).Append(mesh);
		}
		return model;
	}

	std::vector<Mesh> SplitAt(const Mesh& mesh, const std::vector<size_t>& starts) {
		std::vector<Mesh> pieces;
		std::unordered_map<uint32_t, uint32_t> remap;
		for (size_t i = 0; i < starts.size(); i++) {
			const size_t first = starts[i], last = std::min(i + 1 < starts.size() ? starts[i + 1] : mesh.indices.size(), mesh.indices.size());
			if (first >= last) continue;
			Mesh piece;
			remap.clear();
			for (size_t k = first; k < last; k++) {
				const auto source = mesh.indices[k];
				auto [it, added] = remap.try_emplace(source, static_cast<uint32_t>(piece.positions.size()));
				if (added) {
					piece.positions.push_back(mesh.positions[source]);
					if (source < mesh.normals.size()) piece.normals.push_back(mesh.normals[source]);
					if (source < mesh.colors.size()) piece.colors.push_back(mesh.colors[source]);
					if (source < mesh.glow.size()) piece.glow.push_back(mesh.glow[source]);
					if (source < mesh.looks.size()) piece.looks.push_back(mesh.looks[source]);
				}
				piece.indices.push_back(it->second);
			}
			pieces.push_back(std::move(piece));
		}
		return pieces;
	}

	void KeepTriangles(Mesh& mesh, const std::vector<bool>& keep) {
		Mesh kept;
		std::vector<uint32_t> remap(mesh.positions.size(), UINT32_MAX);
		for (size_t t = 0; t < mesh.TriangleCount(); t++) {
			if (t >= keep.size() || !keep[t]) continue;
			for (int k = 0; k < 3; k++) {
				const auto source = mesh.indices[t * 3 + k];
				if (remap[source] == UINT32_MAX) {
					remap[source] = static_cast<uint32_t>(kept.positions.size());
					kept.positions.push_back(mesh.positions[source]);
					if (source < mesh.normals.size()) kept.normals.push_back(mesh.normals[source]);
					if (source < mesh.colors.size()) kept.colors.push_back(mesh.colors[source]);
					if (source < mesh.glow.size()) kept.glow.push_back(mesh.glow[source]);
					if (source < mesh.looks.size()) kept.looks.push_back(mesh.looks[source]);
				}
				kept.indices.push_back(remap[source]);
			}
		}
		mesh = std::move(kept);
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
					if (source < mesh.glow.size()) current.glow.push_back(mesh.glow[source]);
					if (source < mesh.looks.size()) current.looks.push_back(mesh.looks[source]);
				}
				current.indices.push_back(it->second);
			}
		}
		flush();
		return pieces;
	}

	std::vector<Mesh> Divide(const Mesh& mesh, size_t maxVertices, size_t maxTriangles) {
		if (mesh.Empty()) return {};
		if (mesh.positions.size() <= maxVertices && mesh.TriangleCount() <= maxTriangles) return { mesh };

		// Connected pieces (vertices joined by triangles), so a brick's faces stay together
		std::vector<uint32_t> parent(mesh.positions.size());
		for (uint32_t i = 0; i < parent.size(); i++) parent[i] = i;
		const std::function<uint32_t(uint32_t)> find = [&](uint32_t x) {
			while (parent[x] != x) x = parent[x] = parent[parent[x]];
			return x;
		};
		for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
			const auto a = find(mesh.indices[i]);
			parent[find(mesh.indices[i + 1])] = a;
			parent[find(mesh.indices[i + 2])] = a;
		}

		// divide_mesh: vertices below the mean along the longest side of the bounds, and everything linked to them
		glm::vec3 min = mesh.positions[0], max = mesh.positions[0], mean(0.0f);
		for (const auto& p : mesh.positions) {
			min = glm::min(min, p);
			max = glm::max(max, p);
			mean += p;
		}
		mean /= static_cast<float>(mesh.positions.size());
		const auto size = max - min;
		const int axis = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2;
		std::vector<bool> below(mesh.positions.size(), false);
		for (uint32_t v = 0; v < mesh.positions.size(); v++) {
			if (mesh.positions[v][axis] < mean[axis]) below[find(v)] = true;
		}

		Mesh halves[2];
		std::vector<uint32_t> remap(mesh.positions.size(), UINT32_MAX);
		for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
			auto& half = halves[below[find(mesh.indices[i])] ? 1 : 0];
			for (int k = 0; k < 3; k++) {
				const auto source = mesh.indices[i + k];
				if (remap[source] == UINT32_MAX) {
					remap[source] = static_cast<uint32_t>(half.positions.size());
					half.positions.push_back(mesh.positions[source]);
					if (source < mesh.normals.size()) half.normals.push_back(mesh.normals[source]);
					if (source < mesh.colors.size()) half.colors.push_back(mesh.colors[source]);
					if (source < mesh.glow.size()) half.glow.push_back(mesh.glow[source]);
					if (source < mesh.looks.size()) half.looks.push_back(mesh.looks[source]);
				}
				half.indices.push_back(remap[source]);
			}
		}
		// LU Toolbox gives up below a 10% share; this splits the old way then
		const float share = static_cast<float>(halves[1].positions.size()) / static_cast<float>(mesh.positions.size());
		if (std::min(share, 1.0f - share) < 0.1f) return Split(mesh, maxVertices, maxTriangles);
		std::vector<Mesh> pieces;
		for (const auto& half : halves) {
			for (auto& piece : Divide(half, maxVertices, maxTriangles)) pieces.push_back(std::move(piece));
		}
		return pieces;
	}
}
