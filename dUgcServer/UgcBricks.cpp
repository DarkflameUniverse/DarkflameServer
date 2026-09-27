#include "UgcBricks.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

#include "tinyxml2.h"
#include "ZCompression.h"

namespace {
	constexpr int32_t GEOMETRY_MAGIC = 0x42473031; // "10GB"
	constexpr uint32_t MAX_GEOMETRY_PARTS = 64;

	template<typename T>
	bool ReadAt(std::string_view data, size_t offset, T& value) {
		if (offset > data.size() || sizeof(T) > data.size() - offset) return false;
		std::memcpy(&value, data.data() + offset, sizeof(T));
		return true;
	}

	std::string Lower(std::string_view text) {
		std::string out(text);
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}
}

namespace UgcBricks {
	std::optional<Geometry> ParseGeometry(std::string_view data) {
		int32_t magic{}, vertexCount{}, indexCount{}, options{};
		if (!ReadAt(data, 0, magic) || magic != GEOMETRY_MAGIC || !ReadAt(data, 4, vertexCount) || !ReadAt(data, 8, indexCount) ||
			!ReadAt(data, 12, options) || vertexCount < 0 || indexCount < 0 || indexCount % 3 != 0) {
			return std::nullopt;
		}
		const auto vertices = static_cast<size_t>(vertexCount);
		const auto indices = static_cast<size_t>(indexCount);
		size_t offset = 16;
		const bool hasUvs = (options & 3) == 3;
		const size_t needed = offset + vertices * 24 + (hasUvs ? vertices * 8 : 0) + indices * 4;
		if (needed > data.size()) return std::nullopt;

		Geometry geometry;
		geometry.positions.resize(vertices * 3);
		std::memcpy(geometry.positions.data(), data.data() + offset, vertices * 12);
		offset += vertices * 12;
		geometry.normals.resize(vertices * 3);
		std::memcpy(geometry.normals.data(), data.data() + offset, vertices * 12);
		offset += vertices * 12;
		if (hasUvs) offset += vertices * 8;
		geometry.indices.resize(indices);
		std::memcpy(geometry.indices.data(), data.data() + offset, indices * 4);
		for (const auto index : geometry.indices) {
			if (index >= vertices) return std::nullopt;
		}
		return geometry;
	}

	std::map<uint32_t, Material> ParseMaterials(std::string_view xml) {
		std::map<uint32_t, Material> materials;
		tinyxml2::XMLDocument doc;
		if (doc.Parse(xml.data(), xml.size()) != tinyxml2::XML_SUCCESS) return materials;
		const auto* root = doc.FirstChildElement("Materials");
		if (!root) return materials;
		for (const auto* element = root->FirstChildElement("Material"); element; element = element->NextSiblingElement("Material")) {
			const auto id = element->UnsignedAttribute("MatID", 0);
			if (id == 0) continue;
			const auto channel = [element](const char* name, uint32_t fallback) {
				return static_cast<uint8_t>(std::min<uint32_t>(element->UnsignedAttribute(name, fallback), 255));
			};
			materials[id] = Material{ channel("Red", 160), channel("Green", 160), channel("Blue", 160), channel("Alpha", 255) };
		}
		return materials;
	}

	std::optional<std::string> ReadZipEntry(std::string_view zip, std::string_view name) {
		// The end of central directory record is in the last 64 KiB + 22 bytes
		constexpr uint32_t END_SIGNATURE = 0x06054b50;
		constexpr uint32_t CENTRAL_SIGNATURE = 0x02014b50;
		constexpr uint32_t LOCAL_SIGNATURE = 0x04034b50;
		if (zip.size() < 22) return std::nullopt;
		size_t end = std::string_view::npos;
		const size_t lowest = zip.size() > 22 + 65535 ? zip.size() - 22 - 65535 : 0;
		for (size_t i = zip.size() - 22 + 1; i-- > lowest;) {
			uint32_t signature{};
			if (ReadAt(zip, i, signature) && signature == END_SIGNATURE) {
				end = i;
				break;
			}
		}
		if (end == std::string_view::npos) return std::nullopt;
		uint16_t entries{};
		uint32_t directoryOffset{};
		if (!ReadAt(zip, end + 10, entries) || !ReadAt(zip, end + 16, directoryOffset)) return std::nullopt;

		const auto wanted = Lower(name);
		size_t offset = directoryOffset;
		for (uint16_t i = 0; i < entries; i++) {
			uint32_t signature{}, compressedSize{}, size{}, localOffset{};
			uint16_t method{}, nameLength{}, extraLength{}, commentLength{};
			if (!ReadAt(zip, offset, signature) || signature != CENTRAL_SIGNATURE || !ReadAt(zip, offset + 10, method) ||
				!ReadAt(zip, offset + 20, compressedSize) || !ReadAt(zip, offset + 24, size) || !ReadAt(zip, offset + 28, nameLength) ||
				!ReadAt(zip, offset + 30, extraLength) || !ReadAt(zip, offset + 32, commentLength) || !ReadAt(zip, offset + 42, localOffset) ||
				offset + 46 + nameLength > zip.size()) {
				return std::nullopt;
			}
			const auto entryName = zip.substr(offset + 46, nameLength);
			offset += 46 + nameLength + extraLength + commentLength;
			if (Lower(entryName) != wanted) continue;

			uint16_t localNameLength{}, localExtraLength{};
			if (!ReadAt(zip, localOffset, signature) || signature != LOCAL_SIGNATURE || !ReadAt(zip, localOffset + 26, localNameLength) ||
				!ReadAt(zip, localOffset + 28, localExtraLength)) {
				return std::nullopt;
			}
			const size_t dataStart = static_cast<size_t>(localOffset) + 30 + localNameLength + localExtraLength;
			if (dataStart > zip.size() || compressedSize > zip.size() - dataStart) return std::nullopt;
			const auto data = zip.substr(dataStart, compressedSize);
			if (method == 0) return std::string(data);
			if (method == 8) return ZCompression::InflateRaw(data, size);
			return std::nullopt;
		}
		return std::nullopt;
	}

	std::optional<std::filesystem::path> ResolvePath(const std::filesystem::path& root, std::string_view relative) {
		std::string normalized(relative);
		std::replace(normalized.begin(), normalized.end(), '\\', '/');
		std::filesystem::path current = root;
		std::stringstream parts(normalized);
		std::string part;
		std::error_code error;
		while (std::getline(parts, part, '/')) {
			if (part.empty() || part == ".") continue;
			if (part == "..") return std::nullopt;
			if (std::filesystem::exists(current / part, error)) {
				current /= part;
				continue;
			}
			const auto wanted = Lower(part);
			bool found = false;
			for (std::filesystem::directory_iterator it(current, error), endIt; !error && it != endIt; it.increment(error)) {
				if (Lower(it->path().filename().string()) == wanted) {
					current = it->path();
					found = true;
					break;
				}
			}
			if (!found) return std::nullopt;
		}
		if (!std::filesystem::is_regular_file(current, error)) return std::nullopt;
		return current;
	}

	std::optional<std::string> ReadFile(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		if (!file) return std::nullopt;
		std::ostringstream contents;
		contents << file.rdbuf();
		return contents.str();
	}

	BrickLibrary::BrickLibrary(std::filesystem::path res, uint32_t lod, FileReader reader)
		: m_Res(std::move(res)), m_Lod(std::min<uint32_t>(lod, 2)), m_Reader(std::move(reader)) {}

	std::optional<std::string> BrickLibrary::Read(const std::string& relative) const {
		if (m_Reader) {
			if (auto data = m_Reader(relative)) return data;
		}
		const auto path = ResolvePath(m_Res, relative);
		return path ? ReadFile(*path) : std::nullopt;
	}

	bool BrickLibrary::LoadMaterials() {
		const auto zip = Read("brickdb.zip");
		const auto xml = zip ? ReadZipEntry(*zip, "Materials.xml") : std::nullopt;
		if (!xml) return false;
		auto materials = ParseMaterials(*xml);
		if (materials.empty()) return false;
		SetMaterials(std::move(materials));
		return true;
	}

	void BrickLibrary::SetMaterials(std::map<uint32_t, Material> materials) {
		std::lock_guard lock(m_Mutex);
		m_Materials = std::move(materials);
	}

	Material BrickLibrary::GetMaterial(uint32_t id) const {
		// Written once before the workers start, only read after
		const auto it = m_Materials.find(id);
		return it != m_Materials.end() ? it->second : Material{};
	}

	bool BrickLibrary::HasMaterial(uint32_t id) const {
		// Written once before the workers start, only read after
		return m_Materials.contains(id);
	}

	std::shared_ptr<const std::vector<Geometry>> BrickLibrary::GetDesign(uint32_t design, std::optional<uint32_t> lodLevel) {
		const uint32_t lod = std::min<uint32_t>(lodLevel.value_or(m_Lod), 2);
		const uint64_t key = (static_cast<uint64_t>(lod) << 32) | design;
		{
			std::lock_guard lock(m_Mutex);
			if (const auto it = m_Designs.find(key); it != m_Designs.end()) return it->second;
		}
		// Loaded outside the lock; two threads loading the same design at once is harmless
		auto parts = std::make_shared<std::vector<Geometry>>();
		const auto folder = "brickprimitives/lod" + std::to_string(lod) + "/";
		for (uint32_t index = 0; index < MAX_GEOMETRY_PARTS; index++) {
			const auto name = std::to_string(design) + ".g" + (index == 0 ? "" : std::to_string(index));
			const auto data = Read(folder + name);
			if (!data) break;
			auto geometry = ParseGeometry(*data);
			if (!geometry) break;
			parts->push_back(std::move(*geometry));
		}
		if (parts->empty() && lod > 0) return GetDesign(design, lod - 1);
		std::lock_guard lock(m_Mutex);
		return m_Designs.emplace(key, std::move(parts)).first->second;
	}

	size_t BrickLibrary::CachedDesigns() const {
		std::lock_guard lock(m_Mutex);
		return m_Designs.size();
	}
}
