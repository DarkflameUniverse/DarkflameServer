#include "UgcFormats.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

#include "MD5.h"
#include "ZCompression.h"

namespace {
	class Writer {
	public:
		template<typename T>
		void Put(T value) {
			char bytes[sizeof(T)];
			std::memcpy(bytes, &value, sizeof(T));
			m_Data.append(bytes, sizeof(T));
		}
		void U8(uint8_t value) { Put(value); }
		void U16(uint16_t value) { Put(value); }
		void U32(uint32_t value) { Put(value); }
		void I32(int32_t value) { Put(value); }
		void Float(float value) { Put(value); }
		void SizedString(const std::string& value) {
			U32(static_cast<uint32_t>(value.size()));
			m_Data += value;
		}
		void Raw(std::string_view bytes) { m_Data += bytes; }
		std::string& Data() { return m_Data; }

	private:
		std::string m_Data;
	};

	void PutBigEndian(std::string& out, uint32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8) out += static_cast<char>((value >> shift) & 0xFF);
	}

	uint32_t Crc32(std::string_view data) {
		static const auto table = [] {
			std::array<uint32_t, 256> values{};
			for (uint32_t i = 0; i < 256; i++) {
				uint32_t c = i;
				for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
				values[i] = c;
			}
			return values;
		}();
		uint32_t crc = 0xFFFFFFFFu;
		for (const auto byte : data) crc = table[(crc ^ static_cast<uint8_t>(byte)) & 0xFF] ^ (crc >> 8);
		return crc ^ 0xFFFFFFFFu;
	}

	void PngChunk(std::string& out, const char* type, std::string_view data) {
		PutBigEndian(out, static_cast<uint32_t>(data.size()));
		std::string typed(type, 4);
		typed += data;
		out += typed;
		PutBigEndian(out, Crc32(typed));
	}

	// Gamebryo's block writing: a block per call, types and strings collected into the header's tables
	class NifBuilder {
	public:
		int32_t String(const std::string& value) {
			if (value.empty()) return -1;
			const auto it = std::find(m_Strings.begin(), m_Strings.end(), value);
			if (it != m_Strings.end()) return static_cast<int32_t>(it - m_Strings.begin());
			m_Strings.push_back(value);
			return static_cast<int32_t>(m_Strings.size() - 1);
		}

		int32_t Add(const std::string& type, std::string data) {
			auto it = std::find(m_Types.begin(), m_Types.end(), type);
			if (it == m_Types.end()) it = m_Types.insert(m_Types.end(), type);
			m_BlockTypes.push_back(static_cast<uint16_t>(it - m_Types.begin()));
			m_Blocks.push_back(std::move(data));
			return static_cast<int32_t>(m_Blocks.size() - 1);
		}

		// Reserves a block to fill in later (a parent that lists children made after it)
		int32_t Reserve(const std::string& type) { return Add(type, {}); }
		void Fill(int32_t block, std::string data) { m_Blocks[block] = std::move(data); }

		std::string Finish(int32_t root) {
			Writer out;
			out.Raw("Gamebryo File Format, Version 20.3.0.9\n");
			out.U32(0x14030009);
			out.U8(1); // little endian
			out.U32(0); // user version
			out.U32(static_cast<uint32_t>(m_Blocks.size()));
			out.U16(static_cast<uint16_t>(m_Types.size()));
			for (const auto& type : m_Types) out.SizedString(type);
			for (const auto type : m_BlockTypes) out.U16(type);
			for (const auto& block : m_Blocks) out.U32(static_cast<uint32_t>(block.size()));
			out.U32(static_cast<uint32_t>(m_Strings.size()));
			size_t longest = 0;
			for (const auto& value : m_Strings) longest = std::max(longest, value.size());
			out.U32(static_cast<uint32_t>(longest));
			for (const auto& value : m_Strings) out.SizedString(value);
			out.U32(0); // groups
			for (const auto& block : m_Blocks) out.Raw(block);
			out.U32(1); // roots
			out.I32(root);
			return std::move(out.Data());
		}

	private:
		std::vector<std::string> m_Types;
		std::vector<uint16_t> m_BlockTypes;
		std::vector<std::string> m_Blocks;
		std::vector<std::string> m_Strings;
	};

	// NiAVObject flags as the game's own brick models (res/BrickModels/ndmade) have them: nodes 0x110, shapes 0x10
	constexpr uint16_t NODE_FLAGS = 0x110;
	constexpr uint16_t SHAPE_FLAGS = 0x10;

	void WriteNet(Writer& out, int32_t name) {
		out.I32(name);
		out.U32(0);  // extra data
		out.I32(-1); // controller
	}

	void WriteAv(Writer& out, int32_t name, const std::vector<int32_t>& properties, uint16_t flags = NODE_FLAGS) {
		WriteNet(out, name);
		out.U16(flags);
		for (int i = 0; i < 3; i++) out.Float(0.0f); // translation
		for (int row = 0; row < 3; row++) {
			for (int col = 0; col < 3; col++) out.Float(row == col ? 1.0f : 0.0f);
		}
		out.Float(1.0f); // scale
		out.U32(static_cast<uint32_t>(properties.size()));
		for (const auto property : properties) out.I32(property);
		out.I32(-1); // collision object
	}

	std::string TriShapeData(const UgcModel::Mesh& mesh) {
		Writer out;
		const auto count = static_cast<uint16_t>(mesh.positions.size());
		out.I32(0); // group ID
		out.U16(count);
		out.U8(0); // keep flags
		out.U8(0); // compress flags
		out.U8(1); // has vertices
		glm::vec3 min(std::numeric_limits<float>::max()), max(-std::numeric_limits<float>::max());
		for (const auto& p : mesh.positions) {
			out.Float(p.x);
			out.Float(p.y);
			out.Float(p.z);
			min = glm::min(min, p);
			max = glm::max(max, p);
		}
		out.U16(0); // data flags: no texture coordinates or tangents
		const bool normals = mesh.normals.size() == mesh.positions.size();
		out.U8(normals ? 1 : 0);
		if (normals) {
			for (const auto& n : mesh.normals) {
				out.Float(n.x);
				out.Float(n.y);
				out.Float(n.z);
			}
		}
		const glm::vec3 center = mesh.positions.empty() ? glm::vec3(0.0f) : (min + max) * 0.5f;
		float radius = 0.0f;
		for (const auto& p : mesh.positions) radius = std::max(radius, glm::length(p - center));
		out.Float(center.x);
		out.Float(center.y);
		out.Float(center.z);
		out.Float(radius);
		const bool colors = mesh.colors.size() == mesh.positions.size();
		out.U8(colors ? 1 : 0);
		if (colors) {
			for (const auto& c : mesh.colors) {
				out.Float(std::clamp(c.r, 0.0f, 1.0f));
				out.Float(std::clamp(c.g, 0.0f, 1.0f));
				out.Float(std::clamp(c.b, 0.0f, 1.0f));
				out.Float(std::clamp(c.a, 0.0f, 1.0f));
			}
		}
		out.U16(0x4000); // consistency: static
		out.I32(-1);     // additional data
		const auto triangles = static_cast<uint16_t>(mesh.indices.size() / 3);
		out.U16(triangles);
		out.U32(static_cast<uint32_t>(triangles) * 3);
		out.U8(1); // has triangles
		for (size_t i = 0; i < static_cast<size_t>(triangles) * 3; i++) out.U16(static_cast<uint16_t>(mesh.indices[i]));
		out.U16(0); // match groups
		return std::move(out.Data());
	}
	// An NiNode's data: no properties, `children`, no effects
	std::string NodeData(int32_t name, const std::vector<int32_t>& children) {
		Writer node;
		WriteAv(node, name, {});
		node.U32(static_cast<uint32_t>(children.size()));
		for (const auto child : children) node.I32(child);
		node.U32(0); // effects
		return std::move(node.Data());
	}

	// The properties every shape shares, and the shapes
	class SharedProperties {
	public:
		explicit SharedProperties(NifBuilder& nif) : m_Nif(nif) {
			Writer material;
			WriteNet(material, -1);
			for (int i = 0; i < 3; i++) material.Float(1.0f); // ambient
			for (int i = 0; i < 3; i++) material.Float(1.0f); // diffuse
			for (int i = 0; i < 3; i++) material.Float(0.0f); // specular
			for (int i = 0; i < 3; i++) material.Float(0.0f); // emissive
			material.Float(4.0f);  // glossiness, as the game's brick models
			material.Float(1.0f);  // alpha
			m_Material = nif.Add("NiMaterialProperty", std::move(material.Data()));

			Writer vertexColor;
			WriteNet(vertexColor, -1);
			vertexColor.U16((2 << 4) | (1 << 3)); // vertex colors are ambient and diffuse; lit
			m_VertexColor = nif.Add("NiVertexColorProperty", std::move(vertexColor.Data()));
		}

		// An NiTriShape of `mesh` (-1 when it is empty or too big for the format)
		int32_t Shape(const std::string& name, const UgcModel::Mesh* mesh, bool transparent) {
			if (!mesh || mesh->Empty() || mesh->positions.size() > 65535 || mesh->TriangleCount() > 65535) return -1;
			// The properties every shape of the game's own brick models has, in their order: material, alpha (blending
			// by the vertex alpha: 1 on opaque bricks), specular (off) and vertex colors
			if (m_Alpha < 0) {
				Writer alpha;
				WriteNet(alpha, -1);
				alpha.U16(0x00ED); // blend source alpha over one minus source alpha, as the game's files
				alpha.U8(0);
				m_Alpha = m_Nif.Add("NiAlphaProperty", std::move(alpha.Data()));
				Writer specular;
				WriteNet(specular, -1);
				specular.U16(0); // off
				m_Specular = m_Nif.Add("NiSpecularProperty", std::move(specular.Data()));
			}
			(void)transparent;
			std::vector<int32_t> properties{ m_Material, m_Alpha, m_Specular, m_VertexColor };
			const auto shapeBlock = m_Nif.Reserve("NiTriShape");
			const auto dataBlock = m_Nif.Add("NiTriShapeData", TriShapeData(*mesh));
			Writer tri;
			WriteAv(tri, m_Nif.String(name), properties, SHAPE_FLAGS);
			tri.I32(dataBlock);
			tri.I32(-1); // skin instance
			tri.U32(0);  // materials
			tri.I32(-1); // active material
			tri.U8(0);   // material needs update
			m_Nif.Fill(shapeBlock, std::move(tri.Data()));
			return shapeBlock;
		}

	private:
		NifBuilder& m_Nif;
		int32_t m_Material{ -1 };
		int32_t m_VertexColor{ -1 };
		int32_t m_Alpha{ -1 };
		int32_t m_Specular{ -1 };
	};
}

namespace UgcFormats {
	std::string WriteNif(const std::string& rootName, const std::vector<NifShape>& shapes) {
		NifBuilder nif;
		const int32_t root = nif.Reserve("NiNode");
		SharedProperties properties(nif);
		std::vector<int32_t> children;
		for (const auto& shape : shapes) {
			const auto block = properties.Shape(shape.name, shape.mesh, shape.transparent);
			if (block >= 0) children.push_back(block);
		}
		nif.Fill(root, NodeData(nif.String(rootName), children));
		return nif.Finish(root);
	}

	std::string WriteLodNif(const std::string& rootName, const std::vector<NifLodGroup>& groups) {
		NifBuilder nif;
		const int32_t root = nif.Reserve("NiNode");
		SharedProperties properties(nif);
		std::vector<int32_t> groupBlocks;
		for (const auto& group : groups) {
			if (group.lods.empty()) continue;
			const auto lodNode = nif.Reserve("NiLODNode");
			std::vector<int32_t> levels;
			Writer ranges;
			for (int i = 0; i < 3; i++) ranges.Float(0.0f); // LOD center
			ranges.U32(static_cast<uint32_t>(group.lods.size()));
			for (const auto& lod : group.lods) {
				const auto level = nif.Reserve("NiNode");
				std::vector<int32_t> shapes;
				for (const auto* piece : lod.pieces) {
					const auto block = properties.Shape(group.name, piece, group.transparent);
					if (block >= 0) shapes.push_back(block);
				}
				nif.Fill(level, NodeData(nif.String(lod.name), shapes));
				levels.push_back(level);
				ranges.Float(lod.nearDistance);
				ranges.Float(lod.farDistance);
			}
			const auto rangeData = nif.Add("NiRangeLODData", std::move(ranges.Data()));
			auto data = NodeData(nif.String(group.name), levels);
			Writer lod;
			lod.Raw(data);
			lod.U16(3); // switch flags: update only the active child, and controllers (as the game's own files)
			lod.U32(0); // index
			lod.I32(rangeData);
			nif.Fill(lodNode, std::move(lod.Data()));
			groupBlocks.push_back(lodNode);
		}
		nif.Fill(root, NodeData(nif.String(rootName), groupBlocks));
		return nif.Finish(root);
	}

	std::string EncodePng(const UgcRender::Image& image) {
		std::string raw;
		raw.reserve(static_cast<size_t>(image.height) * (image.width * 4 + 1));
		for (int y = 0; y < image.height; y++) {
			raw += '\0'; // no filter
			raw.append(reinterpret_cast<const char*>(image.rgba.data()) + static_cast<size_t>(y) * image.width * 4, static_cast<size_t>(image.width) * 4);
		}
		std::string compressed(ZCompression::GetMaxCompressedLength(static_cast<uint32_t>(raw.size())) + 64, '\0');
		const auto size = ZCompression::Compress(reinterpret_cast<const uint8_t*>(raw.data()), static_cast<uint32_t>(raw.size()),
			reinterpret_cast<uint8_t*>(compressed.data()), static_cast<uint32_t>(compressed.size()));
		if (size <= 0) return {};
		compressed.resize(static_cast<size_t>(size));

		std::string out("\x89PNG\r\n\x1a\n", 8);
		std::string header;
		PutBigEndian(header, static_cast<uint32_t>(image.width));
		PutBigEndian(header, static_cast<uint32_t>(image.height));
		header += std::string("\x08\x06\x00\x00\x00", 5); // 8 bits, RGBA, deflate, no filter method, no interlace
		PngChunk(out, "IHDR", header);
		PngChunk(out, "IDAT", compressed);
		PngChunk(out, "IEND", {});
		return out;
	}

	std::string EncodeDds(const UgcRender::Image& image) {
		std::array<uint32_t, 31> header{};
		header[0] = 124;
		header[1] = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000; // caps, height, width, pitch, pixel format
		header[2] = static_cast<uint32_t>(image.height);
		header[3] = static_cast<uint32_t>(image.width);
		header[4] = static_cast<uint32_t>(image.width) * 4; // pitch
		header[18] = 32;   // pixel format size
		header[19] = 0x41; // RGB with alpha
		header[21] = 32;
		header[22] = 0x00FF0000;
		header[23] = 0x0000FF00;
		header[24] = 0x000000FF;
		header[25] = 0xFF000000;
		header[26] = 0x1000; // texture
		std::string out = "DDS ";
		out.append(reinterpret_cast<const char*>(header.data()), header.size() * 4);
		out.reserve(out.size() + image.rgba.size());
		for (size_t i = 0; i + 3 < image.rgba.size(); i += 4) {
			out += static_cast<char>(image.rgba[i + 2]);
			out += static_cast<char>(image.rgba[i + 1]);
			out += static_cast<char>(image.rgba[i]);
			out += static_cast<char>(image.rgba[i + 3]);
		}
		return out;
	}

	std::string Md5Hex(std::string_view data) {
		MD5 md5;
		md5.update(reinterpret_cast<const unsigned char*>(data.data()), static_cast<MD5::size_type>(data.size()));
		md5.finalize();
		return md5.hexdigest();
	}

	std::string ChecksumXml(std::string_view data) {
		return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<Checksum><MD5>" + Md5Hex(data) + "</MD5><Filesize>" + std::to_string(data.size()) + "</Filesize></Checksum>\n";
	}

	bool ReadChecksumXml(std::string_view xml, std::string& md5, uint32_t& size) {
		const auto between = [xml](std::string_view open, std::string_view close) -> std::string_view {
			const auto start = xml.find(open);
			if (start == std::string_view::npos) return {};
			const auto end = xml.find(close, start + open.size());
			if (end == std::string_view::npos) return {};
			return xml.substr(start + open.size(), end - start - open.size());
		};
		const auto hash = between("<MD5>", "</MD5>");
		const auto length = between("<Filesize>", "</Filesize>");
		if (hash.size() != 32 || length.empty()) return false;
		uint64_t parsed = 0;
		for (const char c : length) {
			if (c < '0' || c > '9') return false;
			parsed = parsed * 10 + static_cast<uint64_t>(c - '0');
			if (parsed > std::numeric_limits<uint32_t>::max()) return false;
		}
		md5 = hash;
		size = static_cast<uint32_t>(parsed);
		return true;
	}
}
