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
		int32_t controller{ -1 };
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
		std::vector<std::vector<float>> m_UvSets; // the UV sets of the geometry being read
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
			net.controller = reader.I32();
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
			if (!reader.Ok()) return;

			// The client puts the object's own position and rotation in place of the root node's (the render component
			// sets them on the loaded root), so a root's stored rotation and translation are never seen; its scale is kept
			auto local = av.transform;
			if (depth == 0) {
				local.r = Transform{}.r;
				local.t = {};
			}
			const auto world = parent.Then(local);
			// Recorded even when hidden: attach points often are
			if (!av.name.empty() && !m_Model.nodes.contains(av.name)) {
				NifFile::NodeTransform node;
				for (int i = 0; i < 9; i++) node.rotation[i] = world.r[i] * world.s;
				node.translation = world.t;
				m_Model.nodes.emplace(av.name, node);
			}
			if (av.flags & APP_CULLED) return;
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
			m_UvSets.clear();
			if (!ReadGeometryData(dataRef, *dataType == "NiTriStripsData", parent.Then(av.transform), mesh) || mesh.indices.empty()) return;
			uint8_t baseSet = 0, darkSet = 0;
			mesh.material = ReadMaterial(properties, baseSet, darkSet);
			// Each texture reads the UV set its flags name (TexturingMapFlags' low byte), the first when that's missing
			const auto set = [this](uint8_t index) { return index < m_UvSets.size() ? m_UvSets[index] : m_UvSets.empty() ? std::vector<float>{} : m_UvSets[0]; };
			mesh.uvs = set(baseSet);
			if (!mesh.material.darkTexture.empty() || mesh.material.embeddedDarkTexture >= 0) mesh.uvs2 = set(darkSet);
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
			for (int set = 0; set < uvSets; set++) m_UvSets.push_back(reader.Array<float>(static_cast<uint64_t>(count) * 2));
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
			std::erase_if(m_UvSets, [count](const std::vector<float>& set) { return set.size() != static_cast<size_t>(count) * 2; });
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

		// A texture slot's NiSourceTexture: an external file name or the block of pixels stored in the file
		void ReadSource(int32_t source, std::string& file, int32_t& embedded) {
			const auto* type = TypeOf(source);
			if (!type || *type != "NiSourceTexture") return;
			m_Used.insert(source);
			auto texture = BlockReader(source);
			ReadNet(texture);
			const auto external = texture.U8();
			const auto name = String(texture.U32());
			const auto pixels = texture.I32();
			const auto* pixelType = TypeOf(pixels);
			if (texture.Ok() && external == 1) file = name;
			else if (texture.Ok() && pixelType && (*pixelType == "NiPixelData" || *pixelType == "NiPersistentSrcTextureRendererData")) {
				embedded = pixels;
				m_Used.insert(pixels);
			}
		}

		// An NiFloatInterpolator's keys (its NiFloatData's) as time, value pairs; empty without data
		std::vector<std::array<float, 2>> FloatKeys(int32_t interpolator) {
			std::vector<std::array<float, 2>> out;
			const auto* interpolatorType = TypeOf(interpolator);
			if (!interpolatorType || *interpolatorType != "NiFloatInterpolator") return out;
			m_Used.insert(interpolator);
			auto value = BlockReader(interpolator);
			value.Float();
			const auto data = value.I32();
			const auto* dataType = TypeOf(data);
			if (!value.Ok() || !dataType || *dataType != "NiFloatData") return out;
			m_Used.insert(data);
			auto keys = BlockReader(data);
			const auto count = keys.U32();
			const auto keyType = count > 0 ? keys.U32() : 0;
			// Linear keys are time and value; quadratic add two tangents; TBC three floats
			const uint32_t floats = keyType == 1 ? 2 : keyType == 2 ? 4 : keyType == 3 ? 5 : 0;
			if (count == 0 || floats == 0 || count > 100000) return out;
			const auto values = keys.Array<float>(static_cast<uint64_t>(count) * floats);
			if (!keys.Ok()) return out;
			for (uint32_t key = 0; key < count; key++) out.push_back({ values[key * floats], values[key * floats + 1] });
			return out;
		}

		// The highest alpha an NiAlphaController among the controllers from `first` on (an NiMaterialProperty's) gives
		// the material: flickering and fading effects often rest at 0 in the file and only show while animated
		std::optional<float> AnimatedAlpha(int32_t first) {
			std::optional<float> highest;
			std::set<int32_t> seen;
			for (int32_t index = first; index >= 0 && !seen.contains(index);) {
				seen.insert(index);
				const auto* type = TypeOf(index);
				if (!type) break;
				auto reader = BlockReader(index);
				const auto next = reader.I32();
				if (*type == "NiAlphaController") {
					m_Used.insert(index);
					reader.Skip(2 + 16); // flags, frequency, phase, start, stop
					reader.I32(); // target
					const auto interpolator = reader.I32();
					if (reader.Ok()) {
						for (const auto& [time, value] : FloatKeys(interpolator)) highest = std::max(highest.value_or(value), value);
					}
				}
				if (!reader.Ok()) break;
				index = next;
			}
			return highest;
		}

		// Tiles a second the controllers from `first` on (an NiTexturingProperty's) move its base map in U and V: each
		// NiTextureTransformController translating the base map, from its NiFloatInterpolator's NiFloatData's first key
		// to its last, times its frequency
		std::array<float, 2> BaseMapScroll(int32_t first) {
			std::array<float, 2> scroll{};
			std::set<int32_t> seen;
			for (int32_t index = first; index >= 0 && !seen.contains(index);) {
				seen.insert(index);
				const auto* type = TypeOf(index);
				if (!type || *type != "NiTextureTransformController") break;
				m_Used.insert(index);
				auto reader = BlockReader(index);
				const auto next = reader.I32();
				reader.U16(); // flags
				const auto frequency = reader.Float();
				reader.Skip(12); // phase, start, stop
				reader.I32(); // target
				const auto interpolator = reader.I32();
				const auto shaderMap = reader.U8();
				const auto slot = reader.U32();
				const auto operation = reader.U32();
				if (reader.Ok() && !shaderMap && slot == 0 && operation <= 1) {
					const auto keys = FloatKeys(interpolator);
					if (keys.size() >= 2) {
						const float duration = keys.back()[0] - keys.front()[0];
						if (duration > 0.0f) scroll[operation] = (keys.back()[1] - keys.front()[1]) / duration * frequency;
					}
				}
				index = next;
			}
			return scroll;
		}

		NifFile::Material ReadMaterial(const Properties& properties, uint8_t& baseSet, uint8_t& darkSet) {
			NifFile::Material material;
			if (properties.material >= 0) {
				auto reader = BlockReader(properties.material);
				const auto controller = ReadNet(reader).controller;
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
				// An animated alpha is drawn at its highest (the views don't play the controller)
				if (const auto animated = AnimatedAlpha(controller)) material.alpha = std::clamp(*animated, 0.0f, 1.0f);
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
				material.uvScroll = BaseMapScroll(ReadNet(reader).controller);
				reader.U16(); // flags
				reader.U32(); // texture count
				// TexDesc (nif.xml, 20.1.0.3 on): source, TexturingMapFlags (clamp in bits 12-15, UV set in 0-7), whether a
				// texture transform follows (translation, scale, rotation, method, center: 32 bytes)
				const auto texDesc = [&reader](int32_t& source, uint16_t& flags) {
					source = reader.I32();
					flags = reader.U16();
					if (reader.U8()) reader.Skip(32);
				};
				int32_t source = -1;
				uint16_t flags = 0;
				if (reader.U8()) { // has base texture
					texDesc(source, flags);
					if (reader.Ok()) {
						ReadSource(source, material.texture, material.embeddedTexture);
						const auto clamp = (flags >> 12) & 0xF;
						material.clampU = clamp == 0 || clamp == 1;
						material.clampV = clamp == 0 || clamp == 2;
						baseSet = static_cast<uint8_t>(flags & 0xFF);
					}
				}
				if (reader.U8()) { // has dark texture
					texDesc(source, flags);
					if (reader.Ok()) {
						ReadSource(source, material.darkTexture, material.embeddedDarkTexture);
						darkSet = static_cast<uint8_t>(flags & 0xFF);
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

	namespace {
		using F = eShaderFamily;
		constexpr auto OPACITY = eTextureAlpha::OPACITY, DECAL = eTextureAlpha::DECAL, IGNORED = eTextureAlpha::IGNORED;

		struct TechniqueRow {
			int32_t shader;
			ShaderTechnique technique;
		};

		/**
		 * Every mapShaders gameValue, by the technique its shader class sets up (ShaderManager's factory table at
		 * 0x01889608, indexed by gameValue; the class's technique setup names it) and the client's res/shaders/*.fx.
		 * Checked in the client: 33 and 82 Technique_Basic_NoLighting_VertColor_NoTexture, 35 and 84
		 * Technique_Basic_NoLighting_VertColor, 37 Technique_Basic_Lighting_VertColor_NoTexture, 38 and 94
		 * Technique_Basic_Lighting_VertColor (94 "Basic": the vtable slot at +0x90 of the class made at 0x0045f240 names
		 * it), 70 Technique_AlphaAsAlpha_UVScrolling_SimpleV_NoLighting_AlphaAnim, 105
		 * Technique_TwoLayersBlended_NoLighting_VertColor_UVScrolling. The rest follow their mapShaders labels ("NL" no
		 * lighting, "NT" no texture, "VC" vertex colors, "AnimUV"/"ScrollingUV" the texture transform, "OneSidedAlpha"
		 * AlphaAsAlpha culled) and the technique of that name in res/shaders.
		 */
		constexpr TechniqueRow TECHNIQUES[] = {
			{ -1, { F::FIXED_FUNCTION, 0, OPACITY, 0 } },
			// TerrainDiffuse.fx's mesh techniques: the texture times the vertex colors and the light, alpha only the fade
			{ 2, { F::TERRAIN, 0, OPACITY, NO_BLEND } },                                     // Terrain Mesh
			{ 3, { F::TERRAIN, 0, IGNORED, RIM_LIGHT | NO_BLEND } },                         // Terrain Mesh Rim Light
			{ 97, { F::TERRAIN, 0, OPACITY, DIFFUSE_ONLY | NO_BLEND } },                     // Terrain Diffuse Map Only
			// LEGOPPLighting: textured alone the texture's alpha is forced to 1; with vertex colors it only lays the
			// texture over them (lerp by its alpha) and the vertex alpha is what shows through
			{ 4, { F::LEGO, 0, DECAL, 0 } },                                               // LEGO (No LOD)
			{ 5, { F::LEGO, 0, DECAL, 0 } },                                               // LEGO
			{ 12, { F::LEGO, 0, DECAL, 0 } },                                              // LEGO-Reveal (the reveal mask left out)
			{ 14, { F::LEGO, 0, OPACITY, NON_DECAL } },                                    // LEGO Masked NonDecal (the specular mask left out)
			{ 19, { F::LEGO, 0, DECAL, 0 } },                                              // Powerups (their own effect, drawn as LEGO)
			{ 20, { F::LEGO, 0, DECAL, 0 } },                                              // Orb (Powerups.fx, drawn as LEGO)
			{ 22, { F::LEGO, EMISSIVE, OPACITY, SUPER_EMISSIVE } },                        // LEGO-SuperEmissive
			{ 25, { F::LEGO, 0, DECAL, 0 } },                                              // LEGO_FrontEnd
			{ 26, { F::LEGO, 0, OPACITY, NON_DECAL } },                                    // LEGO_FaceCreate
			{ 27, { F::LEGO, 0, DECAL, GLOW } },                                           // LEGO-Glow
			{ 28, { F::LEGO, 0, DECAL, GRAYSCALE } },                                      // LEGO-Grayscale
			{ 29, { F::LEGO, 0, DECAL, GLOW | IGNORE_VERTEX_ALPHA } },                     // LEGO-Glow-IgnoreVertAlpha
			{ 30, { F::LEGO, 0, DECAL, UV_ANIM } },                                        // LEGO-AnimUV
			// LEGOPPLighting_Item: texture alpha forced to 1, multiplied by the vertex colors
			{ 31, { F::LEGO, 0, IGNORED, NON_DECAL } },                                    // LEGO-Item
			{ 48, { F::LEGO, 0, IGNORED, NON_DECAL | GLOW } },                             // LEGO-ItemGlow
			{ 50, { F::LEGO, 0, DECAL, 0 } },                                              // LEGO-FadeUp (as when faded in)
			{ 53, { F::LEGO, EMISSIVE, OPACITY, 0 } },                                     // LEGO-Emissive
			{ 72, { F::LEGO, 0, DECAL, SHINY_GLINT } },                                    // ShinyGlint
			{ 88, { F::LEGO, 0, DECAL, NO_AMBIENT } },                                     // LEGO NoAmbient
			{ 92, { F::LEGO, 0, DECAL, 0 } },                                              // Pet Taming LEGO In Cloud
			// LEGOPPLighting's NL pixel shaders: the vertex color or texture as it is
			{ 52, { F::BASIC, UNLIT, OPACITY, ANIM_ALPHA } },                              // LEGO-No Light
			// Darkling: the same lay-over; the dark texture on the second UV set through a window of vertex alphas
			{ 75, { F::DARKLING, 0, DECAL, 0 } },                                          // Darkling
			{ 76, { F::DARKLING, 0, DECAL, SPECULAR } },                                   // Darkling /w Specular
			{ 77, { F::DARKLING, 0, DECAL, NON_DECAL } },                                  // Darkling Structure
			{ 102, { F::DARKLING, 0, DECAL, SHINY_GLINT } },                               // Darking Shiny Glint
			{ 103, { F::DARKLING, 0, DECAL, SPECULAR | SHINY_GLINT } },                    // Darkling /w Specular Shiny Glint
			{ 104, { F::DARKLING, 0, DECAL, NON_DECAL | SHINY_GLINT } },                   // Darkling Structure Shiny Glint
			// AlphaAsAlpha: texture times the (lit) vertex color, both sides
			{ 7, { F::BASIC, 0, OPACITY, DOUBLE_SIDED } },                                 // VertColor_Alpha
			{ 8, { F::BASIC, UNLIT, OPACITY, DOUBLE_SIDED | ANIM_ALPHA } },                // VertColor_NoLighting_Alpha
			{ 9, { F::BASIC, 0, OPACITY, DOUBLE_SIDED } },                                 // VertColor_Alpha_Fade
			{ 10, { F::BASIC, UNLIT, OPACITY, DOUBLE_SIDED | ANIM_ALPHA | BLEND } },       // VertColorTex_NoLight_AlphaBlend
			{ 54, { F::BASIC, UNLIT, OPACITY, DOUBLE_SIDED | ANIM_ALPHA | ALPHA_TEST } },  // VertColorTex_NoLight_AlphaTest
			{ 13, { F::BASIC, 0, OPACITY, UV_ANIM } },                                     // ScrollingUV
			{ 70, { F::BASIC, UNLIT, OPACITY, UV_ANIM | ANIM_ALPHA } },                    // ScrollingUV_NoLight_AnimAlpha
			{ 73, { F::BASIC, UNLIT, OPACITY, UV_ANIM | ANIM_ALPHA } },                    // ScrollingUV_NoLight_AimAlpha_Post
			{ 81, { F::BASIC, UNLIT, OPACITY, UV_ANIM | ANIM_ALPHA | NO_FOG } },           // ScrollingUV NL AnimAlpha NoFog
			// OneSidedAlpha: the same, culled
			{ 55, { F::BASIC, 0, OPACITY, 0 } },                                           // OneSidedAlpha VC
			{ 56, { F::BASIC, UNLIT | NO_VERTEX_COLORS, OPACITY, 0 } },                    // OneSidedAlpha NL
			{ 57, { F::BASIC, UNLIT, OPACITY, ANIM_ALPHA } },                              // OneSidedAlpha NL VC
			{ 58, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, ANIM_ALPHA } },                 // OneSidedAlpha NL VC NT
			{ 59, { F::BASIC, 0, OPACITY, UV_ANIM } },                                     // OneSidedAlpha AnimUV V Skinned
			{ 60, { F::BASIC, 0, OPACITY, 0 } },                                           // OneSidedAlpha VC Skinned
			{ 61, { F::BASIC, UNLIT | NO_VERTEX_COLORS, OPACITY, 0 } },                    // OneSidedAlpha NL Skinned
			{ 62, { F::BASIC, UNLIT, OPACITY, ANIM_ALPHA } },                              // OneSidedAlpha NL VC Skinned
			{ 63, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, ANIM_ALPHA } },                 // OneSidedAlpha NL VC NT Skinned
			{ 64, { F::BASIC, 0, OPACITY, UV_ANIM } },                                     // OneSidedAlpha AnimUV V
			{ 68, { F::BASIC, UNLIT, OPACITY, ANIM_ALPHA } },                              // OneSidedAlpha NL AnimAlpha
			// BasicShaders
			{ 11, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, ANIM_ALPHA } },                 // VertColor_NoLight_NoTex_AnimAlpha
			{ 15, { F::BASIC, UNLIT, OPACITY, 0 } },                                       // VC_NoLighting_2D
			{ 16, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, 0 } },                          // VC_NL_NoTex_2D
			{ 17, { F::BASIC, UNLIT, OPACITY, 0 } },                                       // TV Screen (its static and flicker left out)
			{ 18, { F::BASIC, UNLIT, OPACITY, 0 } },                                       // Head Icon
			{ 23, { F::BASIC, UNLIT, OPACITY, 0 } },                                       // Over Everything (Unlit)
			{ 24, { F::BASIC, UNLIT, OPACITY, NO_FOG | BLEND } },                          // Fogless GrayBubble
			{ 32, { F::BASIC, UNLIT | NO_VERTEX_COLORS | MATERIAL_COLOR, OPACITY, 0 } },   // Basic NL Material
			{ 33, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, ANIM_ALPHA } },                 // Basic NL VC NT
			{ 34, { F::BASIC, UNLIT | NO_VERTEX_COLORS, OPACITY, 0 } },                    // Basic NL
			{ 35, { F::BASIC, UNLIT, OPACITY, 0 } },                                       // Basic NL VC
			{ 36, { F::BASIC, UNLIT | NO_VERTEX_COLORS, OPACITY, UV_ANIM } },              // Basic NL UVAnim
			{ 37, { F::BASIC, NO_TEXTURE, OPACITY, 0 } },                                  // Basic VC NT
			{ 38, { F::BASIC, 0, OPACITY, 0 } },                                           // Basic VC
			{ 39, { F::BASIC, 0, OPACITY, UV_ANIM } },                                     // Basic VC UVAnim
			{ 49, { F::BASIC, 0, OPACITY, 0 } },                                           // Experimental Stub
			{ 65, { F::BASIC, 0, OPACITY, BASIC_EMISSIVE } },                              // VC_Texture_Emissive
			{ 80, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, 0 } },                          // Basic NL NT
			{ 82, { F::BASIC, UNLIT | NO_TEXTURE, OPACITY, NO_BLEND | NO_FOG } },            // Opaque NL VC NT NoFog
			{ 83, { F::BASIC, UNLIT | NO_VERTEX_COLORS, OPACITY, NO_BLEND | NO_FOG } },      // Opaque NL NoFog
			{ 84, { F::BASIC, UNLIT, OPACITY, NO_BLEND | NO_FOG } },                         // Opaque NL VC NoFog
			{ 85, { F::BASIC, NO_TEXTURE, OPACITY, NO_BLEND | NO_FOG } },                    // Opaque VC NT NoFog
			{ 86, { F::BASIC, 0, OPACITY, NO_BLEND | NO_FOG } },                             // Opaque VC NoFog
			{ 87, { F::BASIC, UNLIT, OPACITY, ADDITIVE } },                                // Additive NoLight VertColor
			{ 91, { F::BASIC, UNLIT, OPACITY, BLEND } },                                   // Pet Taming Imagination Cloud
			{ 94, { F::BASIC, 0, OPACITY, 0 } },                                           // Basic
			{ 108, { F::BASIC, UNLIT | NO_VERTEX_COLORS | MATERIAL_COLOR, OPACITY, 0 } },  // Over Everything Material Unlit
			// Two layers (TwoLayersAdded_PS in BasicShaders.fx; the client ships no Technique_TwoLayersBlended_* shader,
			// so the blended ones follow the meshes' data: the dark texture under the base one by the vertex alpha, as
			// Avant Gardens' snow caps and grass fade into rock by it)
			{ 93, { F::BASIC, UNLIT | TWO_LAYERS_ADDED, OPACITY, UV_ANIM } },              // Two Textures Added NL VC AnimUV
			{ 105, { F::BASIC, UNLIT | TWO_LAYERS_BLENDED, OPACITY, UV_ANIM } },           // Two Layers Blended NL VC AnimUV
			{ 106, { F::BASIC, TWO_LAYERS_BLENDED, OPACITY, UV_ANIM } },                   // Two Layers Blended VC AnimUV
			{ 107, { F::BASIC, TWO_LAYERS_ADDED, OPACITY, UV_ANIM } },                     // Two Layers Added VC AnimUV
			// Metallic.fx: both load their reflection cubes themselves (textures/metal)
			{ 98, { F::METAL, REFLECTIVE, OPACITY, 0 } },                                  // Polished Metal
			{ 99, { F::METAL, REFLECTIVE | BRUSHED, OPACITY, 0 } },                        // Brushed Steel
			{ 100, { F::METAL, REFLECTIVE | BRUSHED, OPACITY, 0 } },                       // Brushed Steel Item
			{ 6, { F::CLEAR_PLASTIC, 0, OPACITY, BLEND } },                                // Clear Plastic
			{ 51, { F::BRICK_WATER, 0, OPACITY, 0 } },                                     // BrickWater
			// Ocean.fx
			{ 69, { F::OCEAN, 0, OPACITY, UV_ANIM } },                                     // Distortion (Ocean)
			{ 89, { F::OCEAN, 0, OPACITY, UV_ANIM } },                                     // Distortion Directional (Ocean)
			{ 90, { F::OCEAN, UNLIT, OPACITY, UV_ANIM | OCEAN_FX } },                      // Distortion FX (Ocean)
			{ 95, { F::OCEAN, 0, OPACITY, UV_ANIM | BLEND } },                             // Distortion NoDepth (Ocean) (Alpha)
			{ 101, { F::OCEAN, UNLIT, OPACITY, UV_ANIM } },                                // Distortion (Ocean) Unlit
			{ 78, { F::FLAT_SURF, 0, OPACITY, UV_ANIM } },                                 // Flat Surf
			// Drawn by other passes, not in the world: footprints, post-processing, drop shadows, Technique_Undefined
			{ 21, { F::BASIC, 0, OPACITY, NOT_DRAWN } },                                   // Model Footprint
			{ 71, { F::BASIC, 0, OPACITY, NOT_DRAWN } },                                   // PostProcess Gray Bubble
			{ 74, { F::BASIC, 0, OPACITY, NOT_DRAWN } },                                   // Drop Shadow
			{ 79, { F::BASIC, 0, OPACITY, NOT_DRAWN } },                                   // Post Process Gray Bubble Interior Ghost
			{ 96, { F::BASIC, 0, OPACITY, NOT_DRAWN } },                                   // Undefined
		};
	}

	const char* FamilyName(eShaderFamily family) {
		switch (family) {
			case eShaderFamily::FIXED_FUNCTION: return "fixed";
			case eShaderFamily::LEGO: return "lego";
			case eShaderFamily::BASIC: return "basic";
			case eShaderFamily::METAL: return "metal";
			case eShaderFamily::CLEAR_PLASTIC: return "clearPlastic";
			case eShaderFamily::OCEAN: return "ocean";
			case eShaderFamily::FLAT_SURF: return "flatSurf";
			case eShaderFamily::BRICK_WATER: return "brickWater";
			case eShaderFamily::DARKLING: return "darkling";
			case eShaderFamily::TERRAIN: return "terrain";
		}
		return "lego";
	}

	ShaderTechnique TechniqueFor(int32_t shader) {
		for (const auto& row : TECHNIQUES) {
			if (row.shader == shader) return row.technique;
		}
		return ShaderTechnique{}; // the LEGO shader, as the client falls back to
	}

	std::string TechniquesJson(const std::vector<int32_t>& shaders) {
		nlohmann::json out = nlohmann::json::object();
		for (const auto shader : shaders) {
			const auto technique = TechniqueFor(shader);
			const char* alpha = technique.textureAlpha == eTextureAlpha::DECAL ? "decal" : technique.textureAlpha == eTextureAlpha::IGNORED ? "ignored" : "opacity";
			out[std::to_string(shader)] = { {"family", FamilyName(technique.family)}, {"look", technique.look}, {"alpha", alpha}, {"flags", technique.flags} };
		}
		return out.dump();
	}

	eTextureAlpha TextureAlphaFor(int32_t shader) {
		return TechniqueFor(shader).textureAlpha;
	}

	uint16_t ShaderLookFor(int32_t shader) {
		return TechniqueFor(shader).look;
	}

	std::optional<Model> Parse(std::string_view data, uint32_t lod, std::string& error) {
		return Parser(data, lod).Run(error);
	}

	std::optional<std::string> EmbeddedTexture(std::string_view data, int32_t block) {
		std::string error;
		return Parser(data, 0).Dds(block, error);
	}

	std::string Encode(const Model& model, const std::vector<std::string>& textures, const std::vector<std::string>& darkTextures, const std::vector<uint16_t>& looks) {
		std::string body;
		nlohmann::json meshes = nlohmann::json::array();
		std::vector<std::string> names;
		for (size_t m = 0; m < model.meshes.size(); m++) {
			const auto& mesh = model.meshes[m];
			const auto& material = mesh.material;
			const auto vertices = mesh.positions.size() / 3;
			const std::string texture = m < textures.size() ? textures[m] : std::string{};
			const auto indexOf = [&names](const std::string& name) {
				if (name.empty()) return -1;
				const auto it = std::find(names.begin(), names.end(), name);
				const auto index = static_cast<int32_t>(it - names.begin());
				if (it == names.end()) names.push_back(name);
				return index;
			};
			const int32_t textureIndex = indexOf(texture);
			const int32_t darkIndex = indexOf(m < darkTextures.size() ? darkTextures[m] : std::string{});
			const bool uv2 = darkIndex >= 0 && mesh.uvs2.size() == vertices * 2;
			nlohmann::json entry{
				{"offset", body.size()}, {"vertices", vertices}, {"indices", mesh.indices.size()},
				{"normals", !mesh.normals.empty()}, {"uv", !mesh.uvs.empty() && textureIndex >= 0}, {"colors", !mesh.colors.empty()},
				{"diffuse", Color(material.diffuse)}, {"emissive", Color(material.emissive)}, {"alpha", std::round(material.alpha * 1000.0f) / 1000.0f},
				{"blend", material.alphaBlend}, {"test", material.alphaTest ? material.alphaThreshold : -1}, {"doubleSided", material.doubleSided},
				{"vertexColors", material.vertexColorMode}, {"texture", textureIndex}, {"clampU", material.clampU}, {"clampV", material.clampV},
				{"shaderTag", material.shaderTag}, {"darkTexture", uv2 ? darkIndex : -1}, {"uv2", uv2}
			};
			if (m < looks.size()) entry["look"] = looks[m];
			if (material.uvScroll[0] != 0.0f || material.uvScroll[1] != 0.0f) entry["uvScroll"] = { material.uvScroll[0], material.uvScroll[1] };
			Append(body, mesh.positions.data(), mesh.positions.size() * sizeof(float));
			if (!mesh.normals.empty()) {
				std::vector<int8_t> packed(mesh.normals.size());
				for (size_t i = 0; i < packed.size(); i++) packed[i] = static_cast<int8_t>(std::lround(std::clamp(mesh.normals[i], -1.0f, 1.0f) * 127.0f));
				Append(body, packed.data(), packed.size());
				Pad(body);
			}
			if (entry["uv"].get<bool>()) Append(body, mesh.uvs.data(), mesh.uvs.size() * sizeof(float));
			if (uv2) Append(body, mesh.uvs2.data(), mesh.uvs2.size() * sizeof(float));
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
