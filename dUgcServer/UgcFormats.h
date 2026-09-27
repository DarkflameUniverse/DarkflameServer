#pragma once

#include <array>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "UgcModel.h"
#include "UgcRender.h"

/**
 * The files the UGC server writes: the mesh as a Gamebryo .nif the client loads, icons as .png and .dds, and the
 * client's .checksum files for its downloads. Pure (data in, bytes out).
 */
namespace UgcFormats {
	struct NifShape {
		std::string name;
		const UgcModel::Mesh* mesh{};
		bool transparent{};
	};

	/**
	 * A Gamebryo 20.3.0.9 file (user version 0, as the client's own meshes): a root NiNode named `rootName` with one
	 * NiTriShape per shape (at most 65535 vertices and triangles each: UgcModel::Split), with vertex colors that are
	 * the diffuse and ambient color (NiVertexColorProperty), a white NiMaterialProperty, and for transparent shapes an
	 * NiAlphaProperty blending with the vertex alpha.
	 */
	std::string WriteNif(const std::string& rootName, const std::vector<NifShape>& shapes);

	struct NifLod {
		float nearDistance{};
		float farDistance{};
		std::string name;                      // the level's node, e.g. LOD_0
		std::vector<const UgcModel::Mesh*> pieces; // its shapes (UgcModel::Divide's pieces)
	};

	struct NifLodGroup {
		std::string name; // S01_Opaque_Model: the NiLODNode and its shapes
		bool transparent{};
		std::vector<NifLod> lods;
	};

	/**
	 * The layout LU Toolbox exports (setup_lod_data) and the game's own brick models (res/BrickModels/ndmade) have:
	 * the root node, an NiLODNode per group with NiRangeLODData holding each level's distances, a node per level and
	 * the level's shapes under it, named like the group. Properties as WriteNif.
	 */
	std::string WriteLodNif(const std::string& rootName, const std::vector<NifLodGroup>& groups);

	// PNG (8-bit RGBA)
	std::string EncodePng(const UgcRender::Image& image);

	// DDS as the client's own icons are: DXT5 (BC3), without mipmaps, header flags caps|height|width|pixel format|linear
	// size (0x81007), caps texture (0x1000)
	std::string EncodeDds(const UgcRender::Image& image);

	// One 4x4 block (RGBA, 64 bytes, row by row) as DXT5's 16 bytes: alpha endpoints and 3-bit indices, then the
	// color's two RGB565 endpoints and 2-bit indices
	std::array<uint8_t, 16> EncodeDxt5Block(const std::array<uint8_t, 64>& rgba);

	// Lowercase hex MD5 of `data`
	std::string Md5Hex(std::string_view data);

	// What the client reads from a UGC file's .checksum: the MD5 and size of the file as it is after inflating it
	std::string ChecksumXml(std::string_view data);

	// The MD5 (lowercase hex) and size a .checksum (ChecksumXml) holds; false when it doesn't hold both
	bool ReadChecksumXml(std::string_view xml, std::string& md5, uint32_t& size);
}
