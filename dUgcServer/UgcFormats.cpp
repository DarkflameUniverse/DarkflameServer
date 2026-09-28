#include "UgcFormats.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>

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

	// `glitter`: with a UV set projected for the glitter texture (UgcGlitter::Uv)
	std::string TriShapeData(const UgcModel::Mesh& mesh, const UgcGlitter::Params* glitter = nullptr) {
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
		const bool normals = mesh.normals.size() == mesh.positions.size();
		const bool uvs = glitter && normals;
		out.U16(uvs ? 1 : 0); // data flags: the number of UV sets, no tangents
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
		if (uvs) {
			for (size_t v = 0; v < mesh.positions.size(); v++) {
				const auto uv = UgcGlitter::Uv(mesh.positions[v], mesh.normals[v], glitter->tile);
				out.Float(uv.x);
				out.Float(uv.y);
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
			m_Material = nif.Add("NiMaterialProperty", Material(0.0f));

			Writer vertexColor;
			WriteNet(vertexColor, -1);
			vertexColor.U16((2 << 4) | (1 << 3)); // vertex colors are ambient and diffuse; lit
			m_VertexColor = nif.Add("NiVertexColorProperty", std::move(vertexColor.Data()));
		}

		// A white NiMaterialProperty with this emissive color (grey)
		static std::string Material(float emissive) {
			Writer material;
			WriteNet(material, -1);
			for (int i = 0; i < 3; i++) material.Float(1.0f); // ambient
			for (int i = 0; i < 3; i++) material.Float(1.0f); // diffuse
			for (int i = 0; i < 3; i++) material.Float(0.0f); // specular
			for (int i = 0; i < 3; i++) material.Float(emissive);
			material.Float(4.0f);  // glossiness, as the game's brick models
			material.Float(1.0f);  // alpha
			return std::move(material.Data());
		}

		/**
		 * The glitter groups' NiTexturingProperty (made once a file), as the client's own animated textures have it
		 * (res/mesh/env/env_ag_ocean-maelstrom.nif): the base map wrapping, with a texture transform (Maya method,
		 * center 0.5), its source stored in the file, and NiTextureTransformControllers on the property looping the
		 * transform's translation (flags 0x48: active, looping, app time; frequency 1) through an NiFloatInterpolator
		 * and linear NiFloatData from 0 to 1 tile. The client's NifHasAnimatedControllers (0x00bf4160) finds the
		 * property's first controller and marks the object animated. Apply mode decal: what fixed function would do
		 * with it is what the shader does (the texture over the vertex color by its alpha, the vertex alpha kept).
		 */
		int32_t GlitterTexturing(const UgcGlitter::Params& glitter) {
			if (m_Glitter >= 0) return m_Glitter;
			m_Glitter = m_Nif.Reserve("NiTexturingProperty");
			std::vector<std::pair<uint32_t, float>> motions; // TexTransform (0 translate U, 1 translate V), seconds a tile
			if (glitter.PeriodU() > 0.0f) motions.emplace_back(0, glitter.PeriodU());
			if (glitter.PeriodV() > 0.0f) motions.emplace_back(1, glitter.PeriodV());
			std::vector<int32_t> controllers;
			for (size_t i = 0; i < motions.size(); i++) controllers.push_back(m_Nif.Reserve("NiTextureTransformController"));
			for (size_t i = 0; i < motions.size(); i++) {
				const auto [operation, period] = motions[i];
				Writer data;
				data.U32(2); // keys
				data.U32(1); // linear
				data.Float(0.0f);
				data.Float(0.0f);
				data.Float(period);
				data.Float(1.0f);
				const auto interpolator = m_Nif.Reserve("NiFloatInterpolator");
				const auto dataBlock = m_Nif.Add("NiFloatData", std::move(data.Data()));
				Writer value;
				value.Float(0.0f);
				value.I32(dataBlock);
				m_Nif.Fill(interpolator, std::move(value.Data()));
				Writer controller;
				controller.I32(i + 1 < controllers.size() ? controllers[i + 1] : -1); // next controller
				controller.U16(0x48);     // active, loop, app time (as the client's files)
				controller.Float(1.0f);   // frequency
				controller.Float(0.0f);   // phase
				controller.Float(0.0f);   // start
				controller.Float(period); // stop
				controller.I32(m_Glitter); // target
				controller.I32(interpolator);
				controller.U8(0);  // not a shader map
				controller.U32(0); // the base map
				controller.U32(operation);
				m_Nif.Fill(controllers[i], std::move(controller.Data()));
			}

			// The texture: white, its alpha the flecks, mipmapped, 32-bit as the client's stored textures are (B, G, R, A)
			const auto source = m_Nif.Reserve("NiSourceTexture");
			Writer pixels;
			pixels.U32(1);          // RGBA
			pixels.U8(32);          // bits per pixel
			pixels.U32(0xFFFFFFFF); // renderer hint
			pixels.U32(0);          // extra data
			pixels.U8(1);           // flags
			pixels.U32(0);          // tiling
			pixels.U8(0);           // sRGB
			for (const uint32_t channel : { 2u, 1u, 0u, 3u }) { // blue, green, red, alpha
				pixels.U32(channel);
				pixels.U32(0); // convention: fixed
				pixels.U8(8);
				pixels.U8(0);  // unsigned
			}
			pixels.I32(-1); // palette
			const auto mipmaps = UgcGlitter::Mipmaps(UgcGlitter::FleckAlpha(glitter.flecks));
			pixels.U32(static_cast<uint32_t>(mipmaps.size()));
			pixels.U32(4); // bytes per pixel
			uint32_t offset = 0;
			for (size_t level = 0; level < mipmaps.size(); level++) {
				const uint32_t side = static_cast<uint32_t>(UgcGlitter::TEXTURE_SIZE) >> level;
				pixels.U32(side);
				pixels.U32(side);
				pixels.U32(offset);
				offset += static_cast<uint32_t>(mipmaps[level].size()) * 4;
			}
			pixels.U32(offset); // pixels
			pixels.U32(offset); // padded
			pixels.U32(1);      // faces
			pixels.U32(3);      // platform: DX9
			for (const auto& level : mipmaps) {
				for (const auto a : level) {
					pixels.U8(255);
					pixels.U8(255);
					pixels.U8(255);
					pixels.U8(a);
				}
			}
			const auto pixelData = m_Nif.Add("NiPersistentSrcTextureRendererData", std::move(pixels.Data()));
			Writer texture;
			WriteNet(texture, -1);
			texture.U8(0); // stored in the file
			texture.I32(m_Nif.String("ugc_glitter.dds"));
			texture.I32(pixelData);
			texture.U32(6); // pixel layout: default
			texture.U32(2); // mipmaps: default
			texture.U32(3); // alpha: default
			texture.U8(1);  // static
			texture.U8(0);  // direct render
			texture.U8(1);  // persist render data
			m_Nif.Fill(source, std::move(texture.Data()));

			Writer texturing;
			texturing.I32(-1); // name
			texturing.U32(0);  // extra data
			texturing.I32(controllers.empty() ? -1 : controllers[0]);
			texturing.U16(1 << 1); // apply mode decal
			texturing.U32(9);  // texture slots
			texturing.U8(1);   // base map
			texturing.I32(source);
			texturing.U16(0x3200); // wrap S and T, trilinear, UV set 0
			texturing.U8(1);       // texture transform
			texturing.Float(0.0f); // translation
			texturing.Float(0.0f);
			texturing.Float(1.0f); // scale
			texturing.Float(1.0f);
			texturing.Float(0.0f); // rotation
			texturing.U32(2);      // Maya
			texturing.Float(0.5f); // center
			texturing.Float(0.5f);
			for (int slot = 1; slot < 9; slot++) texturing.U8(0); // dark, detail, gloss, glow, bump, normal, parallax, decal
			texturing.U32(0); // shader maps
			m_Nif.Fill(m_Glitter, std::move(texturing.Data()));
			return m_Glitter;
		}

		// An NiTriShape of `mesh` (-1 when it is empty or too big for the format); `emissive`: its material's
		// emissive color, 0 for the shared material without one; `glitter`: with the glitter texture
		int32_t Shape(const std::string& name, const UgcModel::Mesh* mesh, bool transparent, float emissive = 0.0f, const UgcGlitter::Params* glitter = nullptr) {
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
			int32_t material = m_Material;
			if (emissive > 0.0f) {
				auto [it, added] = m_Emissive.try_emplace(emissive, -1);
				if (added) it->second = m_Nif.Add("NiMaterialProperty", Material(emissive));
				material = it->second;
			}
			std::vector<int32_t> properties{ material, m_Alpha, m_Specular, m_VertexColor };
			if (glitter) properties.push_back(GlitterTexturing(*glitter));
			const auto shapeBlock = m_Nif.Reserve("NiTriShape");
			const auto dataBlock = m_Nif.Add("NiTriShapeData", TriShapeData(*mesh, glitter));
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
		std::map<float, int32_t> m_Emissive; // emissive color -> its material
		int32_t m_Glitter{ -1 };             // the glitter groups' NiTexturingProperty
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
					const auto block = properties.Shape(group.name, piece, group.transparent, group.emissive, group.glitter);
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

	std::array<uint8_t, 16> EncodeDxt5Block(const std::array<uint8_t, 64>& rgba) {
		std::array<uint8_t, 16> out{};

		// Alpha: the block's lowest and highest, eight levels between (a0 > a1), 3 bits per pixel
		uint8_t aMin = 255, aMax = 0;
		for (int i = 0; i < 16; i++) {
			aMin = std::min(aMin, rgba[i * 4 + 3]);
			aMax = std::max(aMax, rgba[i * 4 + 3]);
		}
		out[0] = aMax;
		out[1] = aMin;
		if (aMax != aMin) {
			std::array<int, 8> levels{ aMax, aMin };
			for (int i = 1; i < 7; i++) levels[i + 1] = ((7 - i) * aMax + i * aMin) / 7;
			uint64_t bits = 0;
			for (int i = 0; i < 16; i++) {
				int best = 0, bestError = std::numeric_limits<int>::max();
				for (int l = 0; l < 8; l++) {
					const int error = std::abs(levels[l] - rgba[i * 4 + 3]);
					if (error < bestError) { bestError = error; best = l; }
				}
				bits |= static_cast<uint64_t>(best) << (3 * i);
			}
			for (int i = 0; i < 6; i++) out[2 + i] = static_cast<uint8_t>(bits >> (8 * i));
		}

		// Color: fit along the principal axis of the pixels that show (transparent ones don't count, their color is
		// never seen), then refine the two endpoints by least squares once
		std::array<std::array<float, 3>, 16> px{};
		std::array<bool, 16> used{};
		int count = 0;
		for (int i = 0; i < 16; i++) {
			for (int c = 0; c < 3; c++) px[i][c] = rgba[i * 4 + c];
			used[i] = rgba[i * 4 + 3] > 0;
			count += used[i];
		}
		if (count == 0) used.fill(true), count = 16;
		std::array<float, 3> mean{};
		for (int i = 0; i < 16; i++) if (used[i]) for (int c = 0; c < 3; c++) mean[c] += px[i][c] / count;
		float cov[6]{};
		for (int i = 0; i < 16; i++) {
			if (!used[i]) continue;
			const float r = px[i][0] - mean[0], g = px[i][1] - mean[1], b = px[i][2] - mean[2];
			cov[0] += r * r; cov[1] += r * g; cov[2] += r * b; cov[3] += g * g; cov[4] += g * b; cov[5] += b * b;
		}
		std::array<float, 3> axis{ 1.0f, 1.0f, 1.0f };
		for (int iteration = 0; iteration < 8; iteration++) {
			const std::array<float, 3> next{ cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2],
				cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2], cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2] };
			const float length = std::max({ std::abs(next[0]), std::abs(next[1]), std::abs(next[2]) });
			if (length < 1e-6f) break;
			for (int c = 0; c < 3; c++) axis[c] = next[c] / length;
		}
		float lo = std::numeric_limits<float>::max(), hi = std::numeric_limits<float>::lowest();
		for (int i = 0; i < 16; i++) {
			if (!used[i]) continue;
			const float t = (px[i][0] - mean[0]) * axis[0] + (px[i][1] - mean[1]) * axis[1] + (px[i][2] - mean[2]) * axis[2];
			lo = std::min(lo, t);
			hi = std::max(hi, t);
		}
		const float axisLength2 = axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2];
		std::array<float, 3> e0{}, e1{};
		for (int c = 0; c < 3; c++) {
			e0[c] = mean[c] + axis[c] * hi / std::max(axisLength2, 1e-6f);
			e1[c] = mean[c] + axis[c] * lo / std::max(axisLength2, 1e-6f);
		}
		const auto to565 = [](const std::array<float, 3>& c) {
			const auto q = [](float v, int max) { return static_cast<uint16_t>(std::clamp(static_cast<int>(v / 255.0f * max + 0.5f), 0, max)); };
			return static_cast<uint16_t>((q(c[0], 31) << 11) | (q(c[1], 63) << 5) | q(c[2], 31));
		};
		const auto from565 = [](uint16_t v) {
			return std::array<float, 3>{ ((v >> 11) & 31) * 255.0f / 31.0f, ((v >> 5) & 63) * 255.0f / 63.0f, (v & 31) * 255.0f / 31.0f };
		};
		const auto indicesFor = [&](uint16_t c0, uint16_t c1, uint32_t& bits) {
			const auto a = from565(c0), b = from565(c1);
			std::array<std::array<float, 3>, 4> palette{ a, b };
			for (int c = 0; c < 3; c++) {
				palette[2][c] = (2 * a[c] + b[c]) / 3.0f;
				palette[3][c] = (a[c] + 2 * b[c]) / 3.0f;
			}
			float total = 0.0f;
			bits = 0;
			for (int i = 0; i < 16; i++) {
				int best = 0;
				float bestError = std::numeric_limits<float>::max();
				for (int p = 0; p < 4; p++) {
					float error = 0.0f;
					for (int c = 0; c < 3; c++) error += (palette[p][c] - px[i][c]) * (palette[p][c] - px[i][c]);
					if (error < bestError) { bestError = error; best = p; }
				}
				if (used[i]) total += bestError;
				bits |= static_cast<uint32_t>(best) << (2 * i);
			}
			return total;
		};
		uint16_t c0 = to565(e0), c1 = to565(e1);
		uint32_t bits = 0;
		float error = indicesFor(c0 < c1 ? c1 : c0, c0 < c1 ? c0 : c1, bits);
		if (c0 < c1) std::swap(c0, c1);
		// Least squares on the chosen indices (weights 1, 0, 2/3, 1/3 for the first endpoint)
		{
			static constexpr float W[4] = { 1.0f, 0.0f, 2.0f / 3.0f, 1.0f / 3.0f };
			float aa = 0, bb = 0, ab = 0;
			std::array<float, 3> ax{}, bx{};
			for (int i = 0; i < 16; i++) {
				if (!used[i]) continue;
				const float w = W[(bits >> (2 * i)) & 3], v = 1.0f - w;
				aa += w * w; bb += v * v; ab += w * v;
				for (int c = 0; c < 3; c++) { ax[c] += w * px[i][c]; bx[c] += v * px[i][c]; }
			}
			const float det = aa * bb - ab * ab;
			if (std::abs(det) > 1e-6f) {
				std::array<float, 3> r0{}, r1{};
				for (int c = 0; c < 3; c++) {
					r0[c] = (ax[c] * bb - bx[c] * ab) / det;
					r1[c] = (bx[c] * aa - ax[c] * ab) / det;
				}
				uint16_t n0 = to565(r0), n1 = to565(r1);
				if (n0 < n1) std::swap(n0, n1);
				uint32_t nbits = 0;
				const float nerror = indicesFor(n0, n1, nbits);
				if (nerror < error) { c0 = n0; c1 = n1; bits = nbits; error = nerror; }
			}
		}
		if (c0 == c1) bits = 0; // one color: four-color mode needs c0 > c1, and every index then means c0
		out[8] = static_cast<uint8_t>(c0);
		out[9] = static_cast<uint8_t>(c0 >> 8);
		out[10] = static_cast<uint8_t>(c1);
		out[11] = static_cast<uint8_t>(c1 >> 8);
		for (int i = 0; i < 4; i++) out[12 + i] = static_cast<uint8_t>(bits >> (8 * i));
		return out;
	}

	std::string EncodeDds(const UgcRender::Image& image) {
		const auto width = static_cast<uint32_t>(image.width), height = static_cast<uint32_t>(image.height);
		const uint32_t blocksX = std::max(1u, (width + 3) / 4), blocksY = std::max(1u, (height + 3) / 4);
		std::array<uint32_t, 31> header{};
		header[0] = 124;
		header[1] = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000; // caps, height, width, pixel format, linear size
		header[2] = height;
		header[3] = width;
		header[4] = blocksX * blocksY * 16; // linear size
		header[18] = 32;     // pixel format size
		header[19] = 0x4;    // four CC
		header[20] = 0x35545844; // "DXT5"
		header[26] = 0x1000; // texture
		std::string out = "DDS ";
		out.append(reinterpret_cast<const char*>(header.data()), header.size() * 4);
		out.reserve(out.size() + header[4]);
		for (uint32_t by = 0; by < blocksY; by++) {
			for (uint32_t bx = 0; bx < blocksX; bx++) {
				std::array<uint8_t, 64> block{};
				for (uint32_t y = 0; y < 4; y++) {
					for (uint32_t x = 0; x < 4; x++) {
						// Edge blocks of sizes that aren't a multiple of 4 repeat the last row and column
						const uint32_t sx = std::min(bx * 4 + x, width - 1), sy = std::min(by * 4 + y, height - 1);
						const size_t from = (static_cast<size_t>(sy) * width + sx) * 4;
						if (from + 3 < image.rgba.size()) std::memcpy(&block[(y * 4 + x) * 4], &image.rgba[from], 4);
					}
				}
				const auto encoded = EncodeDxt5Block(block);
				out.append(reinterpret_cast<const char*>(encoded.data()), encoded.size());
			}
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
