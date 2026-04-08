#include "RotatorSubComponent.h"
#include "Entity.h"
#include "SimplePhysicsComponent.h"

#include <cmath>
#include <algorithm>

#include <glm/gtc/quaternion.hpp>

RotatorSubComponent::RotatorSubComponent(Entity* parentEntity, const Path* path)
	: PlatformSubComponent(parentEntity, path) {
}

void RotatorSubComponent::UpdatePositionAlongPath(float deltaTime) {
	// Do the base linear movement (position, velocity, departed message)
	PlatformSubComponent::UpdatePositionAlongPath(deltaTime);

	if (!m_ParentEntity) return;

	// Angular velocity calculation matching client LWOPlatform::SetAngularVelocity:
	// 1. If current and next rotations are the same, just set current rotation and zero angular velocity
	// 2. Otherwise, SLERP to the target rotation based on percent, then compute angular velocity
	//    to reach the next waypoint rotation in the remaining travel time

	if (m_CurrentWaypointRotation == m_NextWaypointRotation) {
		m_ParentEntity->SetRotation(m_CurrentWaypointRotation);
		auto* simplePhysics = m_ParentEntity->GetComponent<SimplePhysicsComponent>();
		if (simplePhysics) {
			simplePhysics->SetAngularVelocity(NiPoint3Constant::ZERO);
		}
		return;
	}

	// SLERP to get the current target rotation (matching client CalculateSlerp)
	NiQuaternion targetRot = glm::slerp(m_CurrentWaypointRotation, m_NextWaypointRotation, m_PercentBetweenPoints);

	// Check if we're already close enough to snap (matching client's angle threshold check)
	NiQuaternion currentRot = m_ParentEntity->GetRotation();
	float dotProduct = glm::dot(targetRot, currentRot);
	if (dotProduct < 0.0f) dotProduct = -dotProduct;
	float angleDiff = std::acos(std::clamp(dotProduct, 0.0f, 1.0f));

	bool snappedToTarget = false;
	if (angleDiff < m_MaxLerpAngle) {
		// Close enough — snap to current rotation, use the actual current rotation for angular vel calc
		snappedToTarget = true;
		targetRot = currentRot;
	} else {
		// Set the SLERP'd rotation on the entity
		m_ParentEntity->SetRotation(targetRot);
	}

	// Calculate remaining travel time for angular velocity
	float remainingTime = 0.0f;
	if (!m_TimeBasedMovement && m_TotalDistance > 0.0f) {
		// Distance-based: calculate remaining time from remaining distance and speeds
		NiPoint3 toNext = m_NextWaypointPosition - m_Position;
		float remainingDist = toNext.Length();
		float currentSpeed = CalculateCurrentSpeed();
		if (currentSpeed > 0.0f) {
			remainingTime = remainingDist / currentSpeed;
		}
	} else if (m_TimeBasedMovement) {
		remainingTime = m_TravelTime - m_MoveTimeElapsed;
	}

	if (remainingTime > 0.0f) {
		// Compute angular velocity from quaternion difference (matching client LWOPhysicsCalcAngularVelocity)
		// Angular velocity = axis * (angle / time)
		NiQuaternion rotDiff = m_NextWaypointRotation * glm::inverse(targetRot);

		// Normalize to ensure valid quaternion
		rotDiff = glm::normalize(rotDiff);

		float angle = 2.0f * std::acos(std::clamp(rotDiff.w, -1.0f, 1.0f));
		if (std::abs(angle) > 0.001f) {
			float sinHalf = std::sqrt(1.0f - rotDiff.w * rotDiff.w);
			NiPoint3 axis;
			if (sinHalf > 0.001f) {
				axis = NiPoint3(rotDiff.x / sinHalf, rotDiff.y / sinHalf, rotDiff.z / sinHalf);
			} else {
				axis = NiPoint3(0.0f, 1.0f, 0.0f);
			}

			m_AngularVelocity = axis * (angle / remainingTime);

			auto* simplePhysics = m_ParentEntity->GetComponent<SimplePhysicsComponent>();
			if (simplePhysics) {
				simplePhysics->SetAngularVelocity(m_AngularVelocity);
			}
		}
	}
}
