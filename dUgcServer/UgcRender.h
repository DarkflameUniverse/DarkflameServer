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

	struct AoOptions {
		bool enabled{ true };
		float distance{ 5.0f };     // LU Toolbox's AO distance (Bake Lighting, AO Only)
		int samples{ 64 };          // rays per vertex (AO Samples)
		float strength{ 1.0f };     // 0 leaves the colors, 1 is the full bake
		float glowStrength{ 6.0f }; // what glowing colors add to the light (Glow Strength 3 x Glow Multiplier 2)
	};

	/**
	 * How an icon is drawn. The values that can be set (settings icon_*, presets and overrides) are listed once, with
	 * their ranges and shipped defaults, in UgcIconParams; the ones here are only what the struct starts with.
	 */
	struct IconOptions {
		int size{ 128 };
		int supersample{ 4 };
		float yawDegrees{ 53.36f };  // camera around the model, from +Z towards +X
		float pitchDegrees{ 19.54f }; // camera above the model
		float fovDegrees{ 39.6f };
		float margin{ 1.03f };       // 1 fills the icon, more leaves a border
		float offsetX{};             // the model moved right by this share of the icon's width (after framing)
		float offsetY{};             // and up by this share of its height
		glm::mat4 modelRotation{ 1.0f }; // applied to the model before the camera looks at it
		float sunYawDegrees{ 21.0f };
		float sunPitchDegrees{ 50.3f };
		float sunStrength{ 2.5f };
		float ambient{ 0.192f };     // the world light (radiance) every face gets
		float fill{ 0.0f };          // a light from the camera (irradiance), what lifts the sides facing the viewer
		float specular{ 0.0f };      // the sun's highlight on the plastic
		float shininess{ 60.0f };    // how tight the highlight is
		float exposure{ 1.0f };      // everything times this before it's written as sRGB
		float contrast{ 1.0f };      // around the middle grey of the sRGB result
		float shadows{ 1.0f };       // how much the sun's shadows darken, 0 to 1
		AoOptions ao{ false, 5.0f, 32, 1.0f, 0.0f };
	};

	// The model drawn from the icon's camera, framed to fit, on a transparent background. `opaqueAo`: the opaque mesh's
	// ambient occlusion (AmbientOcclusion) when it is known already, else it is worked out when options.ao wants it.
	Image RenderIcon(const UgcModel::Model& model, const IconOptions& options, const std::vector<float>* opaqueAo = nullptr);

	struct OptimizeOptions {
		int resolution{ 1024 };   // of each direction's render
		bool removeHidden{ true };
		bool groundPlane{ false }; // LU Toolbox's Use Ground Plane: nothing is seen from below the model
	};

	struct OptimizeResult {
		size_t trianglesBefore{};
		size_t trianglesRemoved{};
		std::vector<bool> kept; // per opaque triangle before: whether it stayed (for UgcModel::KeepTriangles)
	};

	/**
	 * Hidden surface removal: renders the opaque mesh from 42 directions around it and removes the triangles that show
	 * in none of them (with a conservative test, so small visible ones stay). Transparent bricks hide nothing and
	 * aren't touched, as in LU Toolbox.
	 */
	OptimizeResult Optimize(UgcModel::Model& model, const OptimizeOptions& options);

	/**
	 * Ambient occlusion of each vertex of `mesh`: the share of `samples` rays (cosine weighted around the vertex
	 * normal, the same pattern every time) that leave without hitting a triangle of `occluders` within `distance`.
	 * 1 is open, 0 fully hidden.
	 */
	std::vector<float> AmbientOcclusion(const UgcModel::Mesh& mesh, const UgcModel::Mesh& occluders, float distance, int samples);

	/**
	 * LU Toolbox's Bake Lighting with AO Only (its defaults): the opaque mesh's occlusion (transparent bricks are hidden
	 * while baking and not baked) plus the glow colors, multiplied into the vertex colors.
	 */
	std::vector<float> BakeAo(UgcModel::Model& model, const AoOptions& options); // the occlusion used, per opaque vertex

	// The 42 directions Optimize renders from (an icosahedron's corners and edge centres), unit length
	std::vector<glm::vec3> SphereDirections();
}
