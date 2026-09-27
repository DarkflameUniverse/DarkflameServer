#include "UgcIconPose.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

namespace UgcIconPose {
	glm::vec3 CameraDirection(float yawDegrees, float pitchDegrees) {
		const float yaw = glm::radians(yawDegrees), pitch = glm::radians(pitchDegrees);
		return { std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch) };
	}

	glm::vec2 DirectionAngles(const glm::vec3& direction) {
		const float horizontal = std::sqrt(direction.x * direction.x + direction.z * direction.z);
		return { glm::degrees(std::atan2(direction.x, direction.z)), glm::degrees(std::atan2(direction.y, horizontal)) };
	}

	glm::mat4 ModelRotation(float yawDegrees, float pitchDegrees, float rollDegrees) {
		auto rotation = glm::rotate(glm::mat4(1.0f), glm::radians(yawDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
		rotation = glm::rotate(rotation, glm::radians(pitchDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
		return glm::rotate(rotation, glm::radians(rollDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	}

	glm::vec3 RotationAngles(const glm::mat4& m) {
		// Ry*Rx*Rz: m[2][1] (column 2, row 1) is -sin(pitch); as three.js's Euler.setFromRotationMatrix for 'YXZ'
		const float m13 = m[2][0], m23 = m[2][1], m33 = m[2][2];
		const float m21 = m[0][1], m22 = m[1][1], m11 = m[0][0], m31 = m[0][2];
		const float pitch = std::asin(std::clamp(-m23, -1.0f, 1.0f));
		float yaw, roll;
		if (std::abs(m23) < 0.9999999f) {
			yaw = std::atan2(m13, m33);
			roll = std::atan2(m21, m22);
		} else {
			yaw = std::atan2(-m31, m11);
			roll = 0.0f;
		}
		return { glm::degrees(yaw), glm::degrees(pitch), glm::degrees(roll) };
	}

	glm::vec3 Frame::IconPoint(const glm::vec3& position) const {
		const auto clip = viewProjection * glm::vec4(position, 1.0f);
		const float w = clip.w > 1e-6f ? clip.w : 1e-6f;
		return { 0.5f + offset.x + (clip.x / w - centerX) * scale * 0.5f, 0.5f - offset.y - (clip.y / w - centerY) * scale * 0.5f, clip.z / w };
	}

	glm::vec4 Frame::IconRect() const {
		// Inverse of IconPoint at the icon's edges (0 and 1)
		const float half = 2.0f / scale;
		return { centerX + (0.0f - 0.5f - offset.x) * half, centerY + (0.5f - offset.y - 1.0f) * half,
			centerX + (1.0f - 0.5f - offset.x) * half, centerY + (0.5f - offset.y) * half };
	}

	Frame Compute(const std::vector<const std::vector<glm::vec3>*>& positions, const Camera& camera) {
		constexpr float INF = std::numeric_limits<float>::infinity();
		Frame frame;
		glm::vec3 min(INF), max(-INF);
		for (const auto* list : positions) {
			for (const auto& p : *list) {
				min = glm::min(min, p);
				max = glm::max(max, p);
			}
		}
		if (min.x > max.x) {
			frame.center = glm::vec3(0.0f);
			frame.radius = 1.0f;
		} else {
			frame.center = (min + max) * 0.5f;
			frame.radius = std::max(glm::length(max - min) * 0.5f, 0.01f);
		}
		const auto direction = CameraDirection(camera.yawDegrees, camera.pitchDegrees);
		frame.fov = glm::radians(std::clamp(camera.fovDegrees, 1.0f, 120.0f));
		frame.distance = frame.radius / std::sin(frame.fov * 0.5f);
		frame.eye = frame.center + direction * frame.distance;
		const float nearPlane = std::max(frame.distance - frame.radius * 1.5f, frame.distance * 0.01f);
		frame.viewProjection = glm::perspective(frame.fov, 1.0f, nearPlane, frame.distance + frame.radius * 1.5f) *
			glm::lookAt(frame.eye, frame.center, glm::vec3(0.0f, 1.0f, 0.0f));

		float minX = INF, minY = INF, maxX = -INF, maxY = -INF;
		for (const auto* list : positions) {
			for (const auto& p : *list) {
				const auto clip = frame.viewProjection * glm::vec4(p, 1.0f);
				if (clip.w <= 0.0f) continue;
				minX = std::min(minX, clip.x / clip.w);
				maxX = std::max(maxX, clip.x / clip.w);
				minY = std::min(minY, clip.y / clip.w);
				maxY = std::max(maxY, clip.y / clip.w);
			}
		}
		if (minX > maxX) return frame;
		frame.centerX = (minX + maxX) * 0.5f;
		frame.centerY = (minY + maxY) * 0.5f;
		frame.scale = 2.0f / (std::max({ maxX - minX, maxY - minY, 1e-6f }) * std::max(camera.margin, 0.1f));
		frame.offset = { camera.offsetX, camera.offsetY };
		frame.ok = true;
		return frame;
	}
}
