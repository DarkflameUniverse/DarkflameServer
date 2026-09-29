#pragma once

#include <vector>

#include <glm/glm.hpp>

/**
 * Where the icon's camera is and how the model is turned for it, worked out the same way in the icon renderer and in
 * the dashboard's pose editor (static/js/ugc-pose.js mirrors these functions, so its 3D view shows what the icon
 * will). Angles are degrees. Pure.
 *
 * The model is turned first (ModelRotation, about its origin), then the camera looks at the centre of its bounds from
 * CameraDirection, as far away as makes the bounding sphere fill the field of view, and last the picture is cropped to
 * the model's projected bounds (Frame): scaled so the larger side fills the icon less the margin, then shifted.
 */
namespace UgcIconPose {
	// From the model towards the camera: yaw around +Y from +Z towards +X, pitch up from the ground
	glm::vec3 CameraDirection(float yawDegrees, float pitchDegrees);
	// The inverse: {yaw, pitch} of a direction (need not be unit length)
	glm::vec2 DirectionAngles(const glm::vec3& direction);

	// The model's turn: yaw around +Y, then pitch around +X, then roll around +Z (R = Ry * Rx * Rz, three.js's 'YXZ' Euler order)
	glm::mat4 ModelRotation(float yawDegrees, float pitchDegrees, float rollDegrees);
	// The inverse: {yaw, pitch, roll} of a rotation (pitch in -90..90; at +-90 the roll is folded into the yaw)
	glm::vec3 RotationAngles(const glm::mat4& rotation);

	struct Camera {
		float yawDegrees{};
		float pitchDegrees{};
		float fovDegrees{ 40.0f };
		float margin{ 1.0f };  // 1: the model's larger projected side fills the icon
		float offsetX{};       // share of the icon's width the model is moved right
		float offsetY{};       // and up
	};

	struct Frame {
		bool ok{};
		glm::vec3 center{};    // of the model's bounds
		float radius{};        // half their diagonal
		glm::vec3 eye{};
		float fov{};           // radians
		float distance{};
		glm::mat4 viewProjection{ 1.0f };
		float centerX{}, centerY{}; // centre of the projected bounds (NDC)
		float scale{ 1.0f };        // NDC -> icon: 2 / (larger projected side * margin)
		glm::vec2 offset{};

		// A point's place in the icon: x right and y down, 0..1 across it, and its depth (NDC z)
		glm::vec3 IconPoint(const glm::vec3& position) const;
		// The icon's square in the camera's NDC: {minX, minY, maxX, maxY}
		glm::vec4 IconRect() const;
	};

	// The frame of an already turned model's vertices (any number of lists)
	Frame Compute(const std::vector<const std::vector<glm::vec3>*>& positions, const Camera& camera);
}
