#include "NifFile.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <set>

#include "json.hpp"

namespace {
	constexpr uint32_t Version(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { return (a << 24) | (b << 16) | (c << 8) | d; }
	constexpr uint32_t MIN_VERSION = Version(20, 2, 0, 5); // strings in a table and block sizes in the header
	constexpr uint32_t MAX_VERSION = Version(20, 3, 0, 9);
	constexpr uint32_t MAX_BLOCKS = 200000;
	constexpr uint32_t MAX_DEPTH = 64;
	constexpr uint16_t APP_CULLED = 1; // NiAVObject flag: hidden

	// Little-endian reads that fail (and stay failed) instead of running past the end
	class Reader {
	public:
		explicit Reader(std::string_view data) : m_Data(data) {}

		template<typename T>
		T Read() {
			T value{};
			if (!m_Ok || sizeof(T) > m_Data.size() - m_Pos) {
				m_Ok = false;
				return value;
			}
			std::memcpy(&value, m_Data.data() + m_Pos, sizeof(T));
			m_Pos += sizeof(T);
			return value;
		}

		float Float() { return Read<float>(); }
		uint8_t U8() { return Read<uint8_t>(); }
		uint16_t U16() { return Read<uint16_t>(); }
		uint32_t U32() { return Read<uint32_t>(); }
		int32_t I32() { return Read<int32_t>(); }

		void Skip(uint64_t bytes) {
			if (!m_Ok || bytes > m_Data.size() - m_Pos) {
				m_Ok = false;
				return;
			}
			m_Pos += static_cast<size_t>(bytes);
		}

		// `count` values of T, or empty (and failed) if there aren't that many
		template<typename T>
		std::vector<T> Array(uint64_t count) {
			std::vector<T> values;
			if (!m_Ok || count > (m_Data.size() - m_Pos) / sizeof(T)) {
				m_Ok = false;
				return values;
			}
			values.resize(static_cast<size_t>(count));
			std::memcpy(values.data(), m_Data.data() + m_Pos, static_cast<size_t>(count) * sizeof(T));
			m_Pos += static_cast<size_t>(count) * sizeof(T);
			return values;
		}

		std::string SizedString() {
			const auto length = U32();
			if (!m_Ok || length > m_Data.size() - m_Pos) {
				m_Ok = false;
				return {};
			}
			std::string value(m_Data.substr(m_Pos, length));
			m_Pos += length;
			return value;
		}

		bool Ok() const { return m_Ok; }
		size_t Position() const { return m_Pos; }

	private:
		std::string_view m_Data;
		size_t m_Pos = 0;
		bool m_Ok = true;
	};

	// Rotation (row-major, for column vectors), translation and uniform scale: p' = t + s * R p
	struct Transform {
		std::array<float, 9> r{ 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		std::array<float, 3> t{};
		float s{ 1.0f };

		std::array<float, 3> Rotate(const std::array<float, 3>& v) const {
			return { r[0] * v[0] + r[1] * v[1] + r[2] * v[2], r[3] * v[0] + r[4] * v[1] + r[5] * v[2], r[6] * v[0] + r[7] * v[1] + r[8] * v[2] };
		}

		std::array<float, 3> Apply(const std::array<float, 3>& v) const {
			const auto rotated = Rotate(v);
			return { t[0] + s * rotated[0], t[1] + s * rotated[1], t[2] + s * rotated[2] };
		}

		// This transform, then `local` inside it (parent * child)
		Transform Then(const Transform& local) const {
			Transform out;
			for (int row = 0; row < 3; row++) {
				for (int col = 0; col < 3; col++) {
					out.r[row * 3 + col] = r[row * 3] * local.r[col] + r[row * 3 + 1] * local.r[3 + col] + r[row * 3 + 2] * local.r[6 + col];
				}
			}
			out.t = Apply(local.t);
			out.s = s * local.s;
			return out;
		}
	};

	// The properties in effect at a point of the tree: a child's own property of a type replaces its parent's
	struct Properties {
		int32_t material = -1;
		int32_t alpha = -1;
		int32_t texturing = -1;
		int32_t vertexColor = -1;
		int32_t stencil = -1;
		int32_t shaderTag = -1; // the nearest multishader tag ("S05__...") on the way down the tree
	};

	struct NetHeader {
		std::string name;
	};

	struct AvHeader {
		std::string name;
		uint16_t flags{};
		Transform transform;
		std::vector<int32_t> properties;
	};

	class Parser {
	public:
		Parser(std::string_view data, uint32_t lod) : m_Data(data), m_Lod(lod) {}

		std::optional<NifFile::Model> Run(std::string& error) {
			if (!ReadHeader(error)) return std::nullopt;
			m_Model.version = m_Version;

			// The footer lists the roots; the first block is the root when it can't be read
			std::vector<int32_t> roots;
			Reader footer(m_Data.substr(m_FooterStart));
			const auto count = footer.U32();
			if (footer.Ok() && count <= m_Blocks.size()) roots = footer.Array<int32_t>(count);
			if (roots.empty() && !m_Blocks.empty()) roots.push_back(0);

			for (const auto root : roots) Visit(root, Transform{}, Properties{}, 0);

			for (size_t i = 0; i < m_Blocks.size(); i++) {
				if (!m_Used.contains(static_cast<int32_t>(i))) {
					const auto& type = m_Types[m_Blocks[i].type];
					if (!IsDrawnType(type)) m_Model.skipped[type]++;
				}
			}

			bool first = true;
			for (const auto& mesh : m_Model.meshes) {
				for (size_t i = 0; i + 2 < mesh.positions.size(); i += 3) {
					for (int axis = 0; axis < 3; axis++) {
						const auto value = mesh.positions[i + axis];
						m_Model.min[axis] = first ? value : std::min(m_Model.min[axis], value);
						m_Model.max[axis] = first ? value : std::max(m_Model.max[axis], value);
					}
					first = false;
				}
			}
			return std::move(m_Model);
		}

		// The DDS file for pixel data block `index` (see NifFile::EmbeddedTexture)
		std::optional<std::string> Dds(int32_t index, std::string& error) {
			if (!ReadHeader(error)) return std::nullopt;
			const auto* type = TypeOf(index);
			if (!type || (*type != "NiPixelData" && *type != "NiPersistentSrcTextureRendererData")) return std::nullopt;
			auto reader = BlockReader(index);
			const auto format = reader.U32();
			reader.U8();  // bits per pixel
			reader.U32(); // renderer hint
			reader.U32(); // extra data
			reader.U8();  // flags
			const auto tiling = reader.U32();
			if (m_Version >= Version(20, 3, 0, 4)) reader.U8(); // sRGB
			reader.Skip(4 * 10); // channels
			reader.I32(); // palette
			const auto mipCount = reader.U32();
			const auto bytesPerPixel = reader.U32();
			if (!reader.Ok() || mipCount == 0 || mipCount > 16 || tiling != 0) return std::nullopt;
			std::vector<std::array<uint32_t, 3>> mips; // width, height, offset
			for (uint32_t i = 0; i < mipCount; i++) mips.push_back({ reader.U32(), reader.U32(), reader.U32() });
			const auto pixelCount = reader.U32();
			if (*type == "NiPersistentSrcTextureRendererData") {
				reader.U32(); // padded pixel count
				reader.U32(); // faces
				reader.U32(); // platform
			} else {
				reader.U32(); // faces
			}
			std::string_view pixels;
			if (reader.Ok() && pixelCount <= m_Blocks[index].size - reader.Position()) pixels = m_Data.substr(m_Blocks[index].offset + reader.Position(), pixelCount);
			const auto width = mips[0][0], height = mips[0][1];
			if (pixels.empty() || width == 0 || height == 0 || width > 8192 || height > 8192 || mips[0][2] != 0) return std::nullopt;

			// DDS header (Microsoft's DDS_HEADER and DDS_PIXELFORMAT)
			std::array<uint32_t, 31> header{};
			header[0] = 124;
			header[1] = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000; // caps, height, width, pixel format, mipmap count
			header[2] = height;
			header[3] = width;
			header[6] = mipCount;
			header[18] = 32; // pixel format size
			if (format >= 4 && format <= 6) {
				header[19] = 0x4; // four CC
				const char* fourCc = format == 4 ? "DXT1" : format == 5 ? "DXT3" : "DXT5";
				std::memcpy(&header[20], fourCc, 4);
			} else if ((format == 0 && bytesPerPixel == 3) || (format == 1 && bytesPerPixel == 4)) {
				header[19] = format == 1 ? 0x41 : 0x40; // RGB, with alpha
				header[21] = bytesPerPixel * 8;
				header[22] = 0x000000FF; // red first in memory
				header[23] = 0x0000FF00;
				header[24] = 0x00FF0000;
				header[25] = format == 1 ? 0xFF000000 : 0;
			} else {
				return std::nullopt;
			}
			header[26] = 0x1000 | (mipCount > 1 ? 0x400008 : 0); // texture, mipmaps
			std::string out = "DDS ";
			out.append(reinterpret_cast<const char*>(header.data()), header.size() * 4);
			out.append(pixels);
			return out;
		}

	private:
		struct Block {
			uint16_t type{};
			size_t offset{};
			uint32_t size{};
		};

		std::string_view m_Data;
		uint32_t m_Lod;
		uint32_t m_Version{};
		std::vector<std::string> m_Types;
		std::vector<std::string> m_Strings;
		std::vector<Block> m_Blocks;
		size_t m_FooterStart{};
		std::set<int32_t> m_Used;
		std::set<int32_t> m_Visiting;
		NifFile::Model m_Model;

		static bool IsDrawnType(const std::string& type) {
			return type == "NiNode" || type == "NiLODNode" || type == "NiBillboardNode" || type == "NiSwitchNode" ||
				type == "NiTriShape" || type == "NiTriStrips" || type == "NiTriShapeData" || type == "NiTriStripsData" ||
				type == "NiMaterialProperty" || type == "NiAlphaProperty" || type == "NiTexturingProperty" || type == "NiSourceTexture" ||
				type == "NiVertexColorProperty" || type == "NiStencilProperty" || type == "NiRangeLODData" ||
				// Read and ignored: they change nothing a still picture shows
				type == "NiSpecularProperty" || type == "NiZBufferProperty" || type == "NiShadeProperty" || type == "NiStringExtraData";
		}

		bool ReadHeader(std::string& error) {
			const auto newline = m_Data.substr(0, 128).find('\n');
			if (newline == std::string_view::npos || !(m_Data.starts_with("Gamebryo File Format") || m_Data.starts_with("NetImmerse File Format"))) {
				error = "not a Gamebryo file";
				return false;
			}
			Reader header(m_Data.substr(newline + 1));
			m_Version = header.U32();
			if (m_Version < MIN_VERSION || m_Version > MAX_VERSION) {
				error = "unsupported version";
				return false;
			}
			const auto endian = header.U8();
			const auto userVersion = header.U32();
			const auto blockCount = header.U32();
			if (!header.Ok() || endian != 1 || userVersion != 0 || blockCount > MAX_BLOCKS) {
				error = "unsupported header (big-endian or another game's user version)";
				return false;
			}
			const auto typeCount = header.U16();
			for (uint32_t i = 0; i < typeCount && header.Ok(); i++) m_Types.push_back(header.SizedString());
			const auto typeIndex = header.Array<uint16_t>(blockCount);
			const auto sizes = header.Array<uint32_t>(blockCount);
			const auto stringCount = header.U32();
			header.U32(); // longest string
			for (uint32_t i = 0; i < stringCount && header.Ok(); i++) m_Strings.push_back(header.SizedString());
			const auto groupCount = header.U32();
			header.Skip(static_cast<uint64_t>(groupCount) * 4);
			if (!header.Ok()) {
				error = "truncated header";
				return false;
			}

			size_t offset = newline + 1 + header.Position();
			m_Blocks.reserve(blockCount);
			for (uint32_t i = 0; i < blockCount; i++) {
				const uint16_t type = typeIndex[i] & 0x7FFF; // the high bit marks PhysX blocks
				if (type >= m_Types.size() || sizes[i] > m_Data.size() - offset) {
					error = "block " + std::to_string(i) + " is out of range";
					return false;
				}
				m_Blocks.push_back({ type, offset, sizes[i] });
				offset += sizes[i];
			}
			m_FooterStart = offset;
			return true;
		}

		const std::string* TypeOf(int32_t index) const {
			if (index < 0 || static_cast<size_t>(index) >= m_Blocks.size()) return nullptr;
			return &m_Types[m_Blocks[index].type];
		}

		Reader BlockReader(int32_t index) const {
			const auto& block = m_Blocks[index];
			return Reader(m_Data.substr(block.offset, block.size));
		}

		std::string String(uint32_t index) const {
			return index < m_Strings.size() ? m_Strings[index] : std::string{};
		}

		NetHeader ReadNet(Reader& reader) {
			NetHeader net;
			net.name = String(reader.U32());
			const auto extra = reader.U32();
			reader.Skip(static_cast<uint64_t>(extra) * 4);
			reader.I32(); // controller
			return net;
		}

		AvHeader ReadAv(Reader& reader) {
			AvHeader av;
			av.name = ReadNet(reader).name;
			av.flags = reader.U16();
			for (auto& value : av.transform.t) value = reader.Float();
			// Matrix33 is stored m11, m21, m31, m12, ... (nif.xml): column by column
			std::array<float, 9> stored{};
			for (auto& value : stored) value = reader.Float();
			for (int row = 0; row < 3; row++) {
				for (int col = 0; col < 3; col++) av.transform.r[row * 3 + col] = stored[col * 3 + row];
			}
			av.transform.s = reader.Float();
			const auto count = reader.U32();
			av.properties = reader.Array<int32_t>(count);
			reader.I32(); // collision object
			return av;
		}

		Properties Inherit(Properties properties, const std::vector<int32_t>& own) {
			for (const auto ref : own) {
				const auto* type = TypeOf(ref);
				if (!type) continue;
				if (*type == "NiMaterialProperty") properties.material = ref;
				else if (*type == "NiAlphaProperty") properties.alpha = ref;
				else if (*type == "NiTexturingProperty") properties.texturing = ref;
				else if (*type == "NiVertexColorProperty") properties.vertexColor = ref;
				else if (*type == "NiStencilProperty") properties.stencil = ref;
				m_Used.insert(ref);
			}
			return properties;
		}

		void Visit(int32_t index, const Transform& parent, Properties properties, uint32_t depth) {
			const auto* type = TypeOf(index);
			if (!type || depth > MAX_DEPTH || m_Visiting.contains(index)) return;
			m_Visiting.insert(index);
			if (*type == "NiNode" || *type == "NiLODNode" || *type == "NiBillboardNode" || *type == "NiSwitchNode") {
				m_Used.insert(index);
				VisitNode(index, *type, parent, properties, depth);
			} else if (*type == "NiTriShape" || *type == "NiTriStrips") {
				m_Used.insert(index);
				VisitGeometry(index, parent, properties);
			}
			m_Visiting.erase(index);
		}

		void VisitNode(int32_t index, const std::string& type, const Transform& parent, Properties properties, uint32_t depth) {
			auto reader = BlockReader(index);
			const auto av = ReadAv(reader);
			const auto childCount = reader.U32();
			const auto children = reader.Array<int32_t>(childCount);
			const auto effectCount = reader.U32();
			reader.Skip(static_cast<uint64_t>(effectCount) * 4);
			if (!reader.Ok() || (av.flags & APP_CULLED)) return;

			const auto world = parent.Then(av.transform);
			properties = Inherit(properties, av.properties);
			if (const auto tag = NifFile::ShaderTag(av.name); tag >= 0) properties.shaderTag = tag;

			std::vector<int32_t> drawn = children;
			if (type == "NiSwitchNode" || type == "NiLODNode") {
				reader.U16(); // switch flags
				const auto active = reader.U32();
				drawn.clear();
				if (type == "NiSwitchNode") {
					if (reader.Ok() && active < children.size()) drawn.push_back(children[active]);
				} else if (const auto chosen = ChooseLod(reader.I32(), children)) {
					drawn.push_back(*chosen);
				}
			}
			for (const auto child : drawn) Visit(child, world, properties, depth + 1);
		}

		// The child of an NiLODNode for m_Lod: children ordered nearest range first (the most detailed)
		std::optional<int32_t> ChooseLod(int32_t dataRef, const std::vector<int32_t>& children) {
			if (children.empty()) return std::nullopt;
			std::vector<std::pair<float, int32_t>> order;
			const auto* dataType = TypeOf(dataRef);
			if (dataType && *dataType == "NiRangeLODData") {
				m_Used.insert(dataRef);
				auto data = BlockReader(dataRef);
				data.Skip(12); // center
				const auto levels = data.U32();
				for (uint32_t i = 0; i < levels && data.Ok() && i < children.size(); i++) {
					const auto nearExtent = data.Float();
					data.Float(); // far extent
					if (data.Ok()) order.emplace_back(nearExtent, children[i]);
				}
			}
			if (order.size() != children.size()) {
				order.clear();
				for (size_t i = 0; i < children.size(); i++) order.emplace_back(static_cast<float>(i), children[i]);
			}
			std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
			return order[std::min<size_t>(m_Lod, order.size() - 1)].second;
		}

		void VisitGeometry(int32_t index, const Transform& parent, Properties properties) {
			auto reader = BlockReader(index);
			const auto av = ReadAv(reader);
			const auto dataRef = reader.I32();
			const auto skin = reader.I32();
			if (!reader.Ok() || (av.flags & APP_CULLED)) return;
			properties = Inherit(properties, av.properties);
			if (const auto tag = NifFile::ShaderTag(av.name); tag >= 0) properties.shaderTag = tag;
			const auto* dataType = TypeOf(dataRef);
			if (!dataType || (*dataType != "NiTriShapeData" && *dataType != "NiTriStripsData")) return;
			m_Used.insert(dataRef);

			NifFile::Mesh mesh;
			if (!ReadGeometryData(dataRef, *dataType == "NiTriStripsData", parent.Then(av.transform), mesh) || mesh.indices.empty()) return;
			mesh.material = ReadMaterial(properties);
			mesh.material.shaderTag = properties.shaderTag;
			if (skin >= 0) {
				m_Model.skinned++;
				m_Used.insert(skin);
			}
			m_Model.meshes.push_back(std::move(mesh));
		}

		bool ReadGeometryData(int32_t index, bool strips, const Transform& transform, NifFile::Mesh& mesh) {
			auto reader = BlockReader(index);
			reader.I32(); // group ID
			const auto count = reader.U16();
			reader.U8(); // keep flags
			reader.U8(); // compress flags
			if (reader.U8()) {
				const auto vertices = reader.Array<float>(static_cast<uint64_t>(count) * 3);
				mesh.positions.reserve(vertices.size());
				for (size_t i = 0; i + 2 < vertices.size(); i += 3) {
					const auto p = transform.Apply({ vertices[i], vertices[i + 1], vertices[i + 2] });
					mesh.positions.insert(mesh.positions.end(), p.begin(), p.end());
				}
			}
			const auto dataFlags = reader.U16();
			if (reader.U8()) {
				const auto normals = reader.Array<float>(static_cast<uint64_t>(count) * 3);
				mesh.normals.reserve(normals.size());
				for (size_t i = 0; i + 2 < normals.size(); i += 3) {
					auto n = transform.Rotate({ normals[i], normals[i + 1], normals[i + 2] });
					const auto length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
					if (length > 0.0f) for (auto& value : n) value /= length;
					mesh.normals.insert(mesh.normals.end(), n.begin(), n.end());
				}
				if (dataFlags & 4096) reader.Skip(static_cast<uint64_t>(count) * 24); // tangents and bitangents
			}
			reader.Skip(16); // bounding sphere
			if (reader.U8()) {
				const auto colors = reader.Array<float>(static_cast<uint64_t>(count) * 4);
				mesh.colors.reserve(colors.size());
				for (const auto value : colors) mesh.colors.push_back(static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)));
			}
			const auto uvSets = dataFlags & 63;
			if (uvSets > 0) {
				mesh.uvs = reader.Array<float>(static_cast<uint64_t>(count) * 2);
				reader.Skip(static_cast<uint64_t>(uvSets - 1) * count * 8);
			}
			reader.U16(); // consistency flags
			reader.I32(); // additional data
			const auto triangles = reader.U16();
			if (!strips) {
				reader.U32(); // triangle points
				if (reader.U8()) mesh.indices = reader.Array<uint16_t>(static_cast<uint64_t>(triangles) * 3);
			} else {
				const auto stripCount = reader.U16();
				const auto lengths = reader.Array<uint16_t>(stripCount);
				if (reader.U8()) {
					for (const auto length : lengths) {
						const auto points = reader.Array<uint16_t>(length);
						for (size_t i = 2; i < points.size(); i++) {
							const auto a = points[i - 2], b = points[i - 1], c = points[i];
							if (a == b || b == c || a == c) continue;
							if (i % 2 == 0) mesh.indices.insert(mesh.indices.end(), { a, b, c });
							else mesh.indices.insert(mesh.indices.end(), { a, c, b });
						}
					}
				}
			}
			if (!reader.Ok() || mesh.positions.size() != static_cast<size_t>(count) * 3) return false;
			if (mesh.normals.size() != mesh.positions.size()) mesh.normals.clear();
			if (mesh.colors.size() != static_cast<size_t>(count) * 4) mesh.colors.clear();
			if (mesh.uvs.size() != static_cast<size_t>(count) * 2) mesh.uvs.clear();
			// Drop triangles pointing past the vertices
			std::vector<uint16_t> valid;
			valid.reserve(mesh.indices.size());
			for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
				if (mesh.indices[i] < count && mesh.indices[i + 1] < count && mesh.indices[i + 2] < count) {
					valid.insert(valid.end(), { mesh.indices[i], mesh.indices[i + 1], mesh.indices[i + 2] });
				}
			}
			mesh.indices = std::move(valid);
			return true;
		}

		NifFile::Material ReadMaterial(const Properties& properties) {
			NifFile::Material material;
			if (properties.material >= 0) {
				auto reader = BlockReader(properties.material);
				ReadNet(reader);
				reader.Skip(12); // ambient
				std::array<float, 3> diffuse{}, emissive{};
				for (auto& value : diffuse) value = reader.Float();
				reader.Skip(12); // specular
				for (auto& value : emissive) value = reader.Float();
				reader.Float(); // glossiness
				const auto alpha = reader.Float();
				if (reader.Ok()) {
					material.diffuse = diffuse;
					material.emissive = emissive;
					material.alpha = std::clamp(alpha, 0.0f, 1.0f);
				}
			}
			if (properties.alpha >= 0) {
				auto reader = BlockReader(properties.alpha);
				ReadNet(reader);
				const auto flags = reader.U16();
				const auto threshold = reader.U8();
				if (reader.Ok()) {
					material.alphaBlend = flags & 1;
					material.alphaTest = flags & 0x200;
					material.alphaThreshold = threshold;
				}
			}
			if (properties.vertexColor >= 0) {
				auto reader = BlockReader(properties.vertexColor);
				ReadNet(reader);
				const auto flags = reader.U16();
				if (reader.Ok()) material.vertexColorMode = static_cast<uint8_t>((flags >> 4) & 3);
			}
			if (properties.stencil >= 0) {
				auto reader = BlockReader(properties.stencil);
				ReadNet(reader);
				const auto flags = reader.U16();
				if (reader.Ok()) material.doubleSided = ((flags >> 10) & 3) == 3; // DRAW_BOTH
			}
			if (properties.texturing >= 0) {
				auto reader = BlockReader(properties.texturing);
				ReadNet(reader);
				reader.U16(); // flags
				reader.U32(); // texture count
				if (reader.U8()) { // has base texture
					const auto source = reader.I32();
					const auto flags = reader.U16();
					const auto* type = TypeOf(source);
					if (reader.Ok() && type && *type == "NiSourceTexture") {
						m_Used.insert(source);
						auto texture = BlockReader(source);
						ReadNet(texture);
						const auto external = texture.U8();
						const auto file = String(texture.U32());
						const auto pixels = texture.I32();
						const auto* pixelType = TypeOf(pixels);
						if (texture.Ok() && external == 1) material.texture = file;
						else if (texture.Ok() && pixelType && (*pixelType == "NiPixelData" || *pixelType == "NiPersistentSrcTextureRendererData")) {
							material.embeddedTexture = pixels;
							m_Used.insert(pixels);
						}
						const auto clamp = (flags >> 12) & 0xF;
						material.clampU = clamp == 0 || clamp == 1;
						material.clampV = clamp == 0 || clamp == 2;
					}
				}
			}
			return material;
		}
	};

	void Append(std::string& out, const void* data, size_t bytes) {
		out.append(static_cast<const char*>(data), bytes);
	}

	void Pad(std::string& out) {
		while (out.size() % 4) out.push_back('\0');
	}

	nlohmann::json Color(const std::array<float, 3>& color) {
		return { std::round(color[0] * 1000.0f) / 1000.0f, std::round(color[1] * 1000.0f) / 1000.0f, std::round(color[2] * 1000.0f) / 1000.0f };
	}
}

namespace NifFile {
	int32_t ShaderTag(std::string_view name) {
		// The client reads the tag with sscanf: "S%d" at the start of the name, else "_S%d" after the first "_S"
		const auto number = [](std::string_view digits) -> int32_t {
			size_t i = 0;
			while (i < digits.size() && (digits[i] == ' ' || digits[i] == '\t')) i++;
			// A signed number names no mapShaders row, like no number at all
			if (i >= digits.size() || digits[i] < '0' || digits[i] > '9') return -1;
			int32_t value = 0;
			for (; i < digits.size() && digits[i] >= '0' && digits[i] <= '9' && value < 100000; i++) value = value * 10 + (digits[i] - '0');
			return value;
		};
		if (name.starts_with('S')) {
			const auto tag = number(name.substr(1));
			if (tag >= 0) return tag;
		}
		const auto at = name.find("_S");
		if (at == std::string_view::npos || at + 3 >= name.size()) return -1;
		return number(name.substr(at + 2));
	}

	int32_t MultishaderPart(std::optional<int32_t> tagShader) {
		return tagShader && *tagShader >= 3 && *tagShader <= 0x6C ? *tagShader : LEGO_SHADER;
	}

	eTextureAlpha TextureAlphaFor(int32_t shader) {
		switch (shader) {
			// LEGOPPLighting: textured alone the texture's alpha is forced to 1; with vertex colors it only lays the
			// texture over them (lerp by its alpha) and the vertex alpha is what shows through
			case 4: case 5: case 12: case 25: case 27: case 28: case 29: case 30: case 50: case 72: case 88:
			// Darkling: the same lay-over; alpha from lighting or the fade
			case 75: case 76: case 77: case 102: case 103: case 104:
				return eTextureAlpha::DECAL;
			// LEGOPPLighting_Item: texture alpha forced to 1, multiplied by the vertex colors
			case 31: case 48:
			// TerrainMeshLighting_Rim: texture times vertex colors, alpha only the fade
			case 3:
				return eTextureAlpha::IGNORED;
			default:
				return eTextureAlpha::OPACITY;
		}
	}

	std::optional<Model> Parse(std::string_view data, uint32_t lod, std::string& error) {
		return Parser(data, lod).Run(error);
	}

	std::optional<std::string> EmbeddedTexture(std::string_view data, int32_t block) {
		std::string error;
		return Parser(data, 0).Dds(block, error);
	}

	std::string Encode(const Model& model, const std::vector<std::string>& textures) {
		std::string body;
		nlohmann::json meshes = nlohmann::json::array();
		std::vector<std::string> names;
		for (size_t m = 0; m < model.meshes.size(); m++) {
			const auto& mesh = model.meshes[m];
			const auto& material = mesh.material;
			const auto vertices = mesh.positions.size() / 3;
			const std::string texture = m < textures.size() ? textures[m] : std::string{};
			int32_t textureIndex = -1;
			if (!texture.empty()) {
				const auto it = std::find(names.begin(), names.end(), texture);
				textureIndex = static_cast<int32_t>(it - names.begin());
				if (it == names.end()) names.push_back(texture);
			}
			nlohmann::json entry{
				{"offset", body.size()}, {"vertices", vertices}, {"indices", mesh.indices.size()},
				{"normals", !mesh.normals.empty()}, {"uv", !mesh.uvs.empty() && textureIndex >= 0}, {"colors", !mesh.colors.empty()},
				{"diffuse", Color(material.diffuse)}, {"emissive", Color(material.emissive)}, {"alpha", std::round(material.alpha * 1000.0f) / 1000.0f},
				{"blend", material.alphaBlend}, {"test", material.alphaTest ? material.alphaThreshold : -1}, {"doubleSided", material.doubleSided},
				{"vertexColors", material.vertexColorMode}, {"texture", textureIndex}, {"clampU", material.clampU}, {"clampV", material.clampV},
				{"shaderTag", material.shaderTag}
			};
			Append(body, mesh.positions.data(), mesh.positions.size() * sizeof(float));
			if (!mesh.normals.empty()) {
				std::vector<int8_t> packed(mesh.normals.size());
				for (size_t i = 0; i < packed.size(); i++) packed[i] = static_cast<int8_t>(std::lround(std::clamp(mesh.normals[i], -1.0f, 1.0f) * 127.0f));
				Append(body, packed.data(), packed.size());
				Pad(body);
			}
			if (entry["uv"].get<bool>()) Append(body, mesh.uvs.data(), mesh.uvs.size() * sizeof(float));
			if (!mesh.colors.empty()) Append(body, mesh.colors.data(), mesh.colors.size());
			Append(body, mesh.indices.data(), mesh.indices.size() * sizeof(uint16_t));
			Pad(body);
			meshes.push_back(std::move(entry));
		}
		nlohmann::json header{
			{"version", 1}, {"meshes", meshes}, {"textures", names},
			{"min", { model.min[0], model.min[1], model.min[2] }}, {"max", { model.max[0], model.max[1], model.max[2] }}
		};
		auto text = header.dump();
		while (text.size() % 4) text.push_back(' ');
		std::string out;
		const auto length = static_cast<uint32_t>(text.size());
		Append(out, &length, sizeof(length));
		out += text;
		out += body;
		return out;
	}

	std::optional<std::string> KfmModelPath(std::string_view data) {
		const auto newline = data.substr(0, 128).find('\n');
		if (newline == std::string_view::npos || data.substr(0, newline).find("KFM") == std::string_view::npos) return std::nullopt;
		Reader reader(data.substr(newline + 1));
		reader.U8(); // little endian
		auto path = reader.SizedString();
		if (!reader.Ok() || path.empty()) return std::nullopt;
		return path;
	}
}
