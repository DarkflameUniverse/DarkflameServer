#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "UgcGlitter.h"
#include "UgcModel.h"
#include "UgcRays.h"

/**
 * A small software rasterizer (no GPU or display needed) for what the UGC server draws: the icons, and the occlusion
 * rays that bake ambient occlusion into the vertex colors.
 */
namespace UgcRender {
	// 8-bit RGBA, rows top to bottom, not premultiplied
	struct Image {
		int width{};
		int height{};
		std::vector<uint8_t> rgba;
	};

	// Whether an icon is denoised (the denoise setting): off, or with Intel Open Image Denoise when the build has it
	enum class eDenoise : uint8_t { OFF = 0, OIDN };

	// The setting's name (off, oidn)
	std::string_view Name(eDenoise denoise);
	// A value by its name; nullopt for anything else
	std::optional<eDenoise> ParseDenoise(std::string_view name);
	// Whether this build can denoise that way (off always)
	bool Available(eDenoise denoise);

	struct AoOptions {
		bool enabled{ true };
		float distance{ 5.0f };     // LU Toolbox's AO distance (Bake Lighting, AO Only)
		int samples{ 64 };          // rays per vertex (AO Samples)
		float strength{ 1.0f };     // 0 leaves the colors, 1 is the full bake
		float glowStrength{ 6.0f }; // what glowing colors add to the light (Glow Strength 3 x Glow Multiplier 2)
		UgcRays::eBackend rays{};   // what traces the occlusion rays (ray_backend)
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
		float modelYawDegrees{};     // the model turned (UgcIconPose::ModelRotation), after modelRotation
		float modelPitchDegrees{};
		float modelRollDegrees{};
		glm::mat4 modelRotation{ 1.0f }; // the model's own turn before that (a build type's AdditionalModelRotation)
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
		float glowEmissive{ 1.0f };  // how far glowing shapes go from lit to their plain color (the glow_emissive setting)
		UgcGlitter::Params glitter;  // the glitter's flecks (glitter_size, glitter_density), drawn where they are at the start
		eDenoise denoise{};          // the finished icon denoised (denoise); off when the build can't
		int denoiseSamples{ 4 };     // denoised: occlusion rays per pixel (of the supersampled image) traced on the model before its bake
		float bakedAo{ 1.0f };       // denoised: the bake's strength (ao_strength; 0 when bake_ao is off), for the traced occlusion
	};

	// The model drawn from the icon's camera, framed to fit, on a transparent background. `opaqueAo`: the opaque mesh's
	// ambient occlusion (AmbientOcclusion) when it is known already, else it is worked out when options.ao wants it.
	// Opaque vertices with a look (UgcModel::Mesh::looks) are drawn roughly as the game's shaders draw them: GLOW goes
	// from lit to its plain color by glowEmissive (LEGO-Emissive), METAL and BRUSHED dim the diffuse light and add a
	// sky-and-ground reflection tinted by the color and a highlight, sharp for polished metal and broad for brushed
	// steel (Polished Metal, Brushed Steel: an environment map tinted by the vertex color). GLITTER vertices (opaque or
	// transparent) get the glitter texture's white flecks over their color before the light (LEGO-AnimUV), still.
	// With options.denoise (and a build that has it) the image is denoised by Open Image Denoise, guided by the colors
	// before the light and the normals (which have no noise). A denoiser only removes noise that differs from pixel to
	// pixel, not the baked occlusion's (which is per vertex), so with `plain` (the same model with the colors it had
	// before its occlusion was baked in) that is drawn instead, with its occlusion traced per pixel: options.
	// denoiseSamples rays (options.ao's distance, strength and ray backend) from each pixel of the supersampled image,
	// noisy, then denoised. Without `plain` it is only denoised.
	Image RenderIcon(const UgcModel::Model& model, const IconOptions& options, const std::vector<float>* opaqueAo = nullptr, const UgcModel::Model* plain = nullptr);

	/**
	 * The fast hidden-face test (hsr_method=fast, what the UGC server did before it traced LU Toolbox's paths): the
	 * opaque mesh rendered from 42 directions around the whole model (SphereDirections), `resolution` pixels square;
	 * per opaque triangle whether it shows in any of them (with a conservative test, so small visible ones stay).
	 * Faces seen only by bounced light (interiors, recesses) don't show. `groundPlane`: nothing is seen from below.
	 */
	std::vector<bool> VisibleFromAround(const UgcModel::Model& model, int resolution, bool groundPlane);

	// The 42 directions VisibleFromAround renders from (an icosahedron's corners and edge centres), unit length
	std::vector<glm::vec3> SphereDirections();

	/**
	 * Ambient occlusion of each vertex of `mesh`: the share of `samples` rays (cosine weighted around the vertex
	 * normal, the same pattern every time) that leave without hitting a triangle of `occluders` within `distance`.
	 * 1 is open, 0 fully hidden.
	 */
	std::vector<float> AmbientOcclusion(const UgcModel::Mesh& mesh, const UgcModel::Mesh& occluders, float distance, int samples,
		UgcRays::eBackend rays = UgcRays::eBackend::BUILTIN);

	/**
	 * LU Toolbox's Bake Lighting with AO Only (its defaults): the opaque mesh's occlusion (transparent bricks are hidden
	 * while baking and not baked) plus the glow colors, multiplied into the vertex colors.
	 */
	std::vector<float> BakeAo(UgcModel::Model& model, const AoOptions& options); // the occlusion used, per opaque vertex
}
