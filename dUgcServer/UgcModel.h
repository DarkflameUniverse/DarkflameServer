#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "UgcBricks.h"

namespace NifFile {
	struct Model;
}

/**
 * Brick models as triangle meshes: LXFML parts, the mesh built from them (opaque and transparent bricks apart, with
 * the material colors as vertex colors) and the mesh of a client .nif. Pure apart from BrickLibrary's file reads.
 */
namespace UgcModel {
	// A brick in a model: its primitive, materials (one per geometry part) and where it is
	struct Part {
		uint32_t designId{};
		std::vector<uint32_t> materials;
		glm::mat4 transform{ 1.0f };
	};

	// The parts of an LXFML 5 (Bricks/Brick/Part with bones) or 4 (Scene/Model/Group/Part with axis angles) model.
	// Empty with `error` set when it can't be read.
	std::vector<Part> ParseLxfml(std::string_view lxfml, std::string& error);

	struct Mesh {
		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec4> colors; // sRGB, 0..1, alpha is opacity
		std::vector<glm::vec3> glow;   // linear glow color per vertex (LU Toolbox's "Glow" layer); empty when nothing glows
		std::vector<uint32_t> indices;

		size_t TriangleCount() const { return indices.size() / 3; }
		bool Empty() const { return indices.empty(); }
		void Append(const Mesh& other);
		void Transform(const glm::mat4& transform);
	};

	struct Model {
		Mesh opaque;
		Mesh transparent;
		std::vector<uint32_t> missingDesigns; // designs without geometry in the client, skipped
		std::vector<size_t> transparentBricks; // where each transparent brick's triangles start in transparent.indices
		size_t bricks{};

		bool Empty() const { return opaque.Empty() && transparent.Empty(); }
		// The bounds of every vertex; false when there are none
		bool Bounds(glm::vec3& min, glm::vec3& max) const;
	};

	enum class ePalette {
		LU_TOOLBOX, // LU Toolbox's colors (UgcPalette), what its importer colors models with
		BRICKDB,    // the brick database's Materials.xml
	};

	struct BuildOptions {
		ePalette palette{ ePalette::LU_TOOLBOX };
		float colorVariation{ 5.0f };      // percent, 0: none (LU Toolbox: Apply Color Variation, 5%)
		uint64_t seed{};                   // of the variation's random numbers
		float transparentOpacity{ 58.82f }; // percent, transparent bricks' vertex alpha (LU Toolbox palette only)
		bool icon{};                       // the icon renderer's color corrections
		uint32_t lod{};                    // brickprimitives level
	};

	/**
	 * The mesh of a model's parts, colored as LU Toolbox's Process Model does: a brick is transparent only when all of
	 * its materials are, each material of each brick has its brightness shifted by the color variation (the same
	 * random number for a brick's material in every LOD and every time), vertex colors are sRGB with alpha 1 for
	 * opaque bricks and the transparent opacity for transparent ones.
	 */
	Model Build(const std::vector<Part>& parts, UgcBricks::BrickLibrary& library, const BuildOptions& options = {});

	/**
	 * The distance range (near, far) of each LOD LU Toolbox makes, for the brickprimitives levels in `used` (0 to 3),
	 * from its settings (lod0..lod3, cull): its setup_lod_data, which picks the ranges by which levels are there.
	 * {0, 0} for a level it has no range for.
	 */
	struct LodDistances {
		float lod0{ 0.0f };
		float lod1{ 50.0f };
		float lod2{ 100.0f };
		float lod3{ 280.0f };
		float cull{ 10000.0f };
	};
	std::vector<std::pair<float, float>> LodRanges(const std::vector<uint32_t>& used, const LodDistances& distances);

	/**
	 * Splits a mesh the way LU Toolbox's divide_mesh does while it has too many vertices (or triangles): at the mean
	 * of its vertices along its longest side, keeping connected pieces whole. Falls back to Split when that can't
	 * divide it.
	 */
	std::vector<Mesh> Divide(const Mesh& mesh, size_t maxVertices = 65535, size_t maxTriangles = 65535);

	// A client .nif's meshes as one model (vertex colors times material color; transparent when blended)
	Model FromNif(const NifFile::Model& nif);

	// The mesh cut into pieces at these index offsets (each piece's triangles start at one), e.g. one per brick
	std::vector<Mesh> SplitAt(const Mesh& mesh, const std::vector<size_t>& starts);

	// Keeps the triangles whose flag is set (and the vertices they use)
	void KeepTriangles(Mesh& mesh, const std::vector<bool>& keep);

	// Splits a mesh into pieces the .nif format can hold (at most `maxVertices` vertices and `maxTriangles` triangles)
	std::vector<Mesh> Split(const Mesh& mesh, size_t maxVertices = 65535, size_t maxTriangles = 65535);
}
