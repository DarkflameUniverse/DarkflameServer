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
		size_t bricks{};

		bool Empty() const { return opaque.Empty() && transparent.Empty(); }
		// The bounds of every vertex; false when there are none
		bool Bounds(glm::vec3& min, glm::vec3& max) const;
	};

	// The mesh of a model's parts
	Model Build(const std::vector<Part>& parts, UgcBricks::BrickLibrary& library);

	// A client .nif's meshes as one model (vertex colors times material color; transparent when blended)
	Model FromNif(const NifFile::Model& nif);

	// Splits a mesh into pieces the .nif format can hold (at most `maxVertices` vertices and `maxTriangles` triangles)
	std::vector<Mesh> Split(const Mesh& mesh, size_t maxVertices = 65535, size_t maxTriangles = 65535);
}
