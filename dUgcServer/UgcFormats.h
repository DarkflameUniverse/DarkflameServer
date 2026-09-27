#pragma once

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

	// PNG (8-bit RGBA)
	std::string EncodePng(const UgcRender::Image& image);

	// DDS, uncompressed 32-bit BGRA without mipmaps
	std::string EncodeDds(const UgcRender::Image& image);

	// Lowercase hex MD5 of `data`
	std::string Md5Hex(std::string_view data);

	// What the client reads from a UGC file's .checksum: the MD5 and size of the file as it is after inflating it
	std::string ChecksumXml(std::string_view data);
}
