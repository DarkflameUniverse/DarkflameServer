#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "UgcModel.h"

/**
 * A small software rasterizer (no GPU or display needed) for what the UGC server draws: the icons, and the renders
 * from many directions that find the faces nobody can see and bake ambient occlusion into the vertex colors.
 */
namespace UgcRender {
	// 8-bit RGBA, rows top to bottom, not premultiplied
	struct Image {
		int width{};
		int height{};
		std::vector<uint8_t> rgba;
	};

	struct IconOptions {
		int size{ 128 };
		int supersample{ 4 };
		float yawDegrees{ 35.0f };   // camera around the model, from +Z towards +X
		float pitchDegrees{ 25.0f }; // camera above the model
		float fovDegrees{ 30.0f };
		float margin{ 1.03f };       // 1 fills the icon, more leaves a border
		glm::mat4 modelRotation{ 1.0f }; // applied to the model before the camera looks at it
	};

	// The model drawn from the icon's camera, framed to fit, on a transparent background
	Image RenderIcon(const UgcModel::Model& model, const IconOptions& options);

	struct OptimizeOptions {
		int resolution{ 1024 };   // of each direction's render
		bool removeHidden{ true };
		bool bakeAo{ true };
		float aoStrength{ 0.6f }; // 0 leaves the colors, 1 makes fully hidden corners black
	};

	struct OptimizeResult {
		size_t trianglesBefore{};
		size_t trianglesRemoved{};
	};

	/**
	 * Renders the opaque mesh from 42 directions around it: triangles that show in none of them are removed (with a
	 * conservative test, so small visible ones stay), and each vertex's share of the directions it can be seen from
	 * darkens its color (ambient occlusion). Transparent bricks hide nothing but are darkened too.
	 */
	OptimizeResult Optimize(UgcModel::Model& model, const OptimizeOptions& options);

	// The 42 directions Optimize renders from (an icosahedron's corners and edge centres), unit length
	std::vector<glm::vec3> SphereDirections();
}
