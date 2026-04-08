#include "SimpleMoverSubComponent.h"
#include "Zone.h"

SimpleMoverSubComponent::SimpleMoverSubComponent(Entity* parentEntity, const NiPoint3& startPos,
	const NiQuaternion& startRot, const NiPoint3& platformMove, float platformMoveTime)
	: PlatformSubComponent(parentEntity, nullptr) {
	GeneratePath(startPos, startRot, platformMove, platformMoveTime);
	m_Path = m_GeneratedPath.get();
	m_TimeBasedMovement = false;

	// Auto-activate and set up initial segment (matching client GenerateSimpleMoverPath)
	if (m_Path && !m_Path->pathWaypoints.empty()) {
		m_Active = true;
		SetupWaypointSegment(m_CurrentWaypointIndex);
	}
}

void SimpleMoverSubComponent::GeneratePath(const NiPoint3& startPos, const NiQuaternion& startRot,
	const NiPoint3& platformMove, float platformMoveTime) {

	auto path = std::make_unique<Path>();
	path->pathType = PathType::MovingPlatform;
	path->pathBehavior = PathBehavior::Once;
	path->pathName = "SimpleMoverPath";
	path->movingPlatform.timeBasedMovement = 0;

	// Waypoint 0: start position
	PathWaypoint wp0;
	wp0.position = startPos;
	wp0.rotation = startRot;
	wp0.movingPlatform.wait = 0.0f;

	// Waypoint 1: start + rotated platformMove
	NiPoint3 move = platformMove;
	NiPoint3 rotatedMove = move.RotateByQuaternion(startRot);
	PathWaypoint wp1;
	wp1.position = startPos + rotatedMove;
	wp1.rotation = startRot;
	wp1.movingPlatform.wait = 0.0f;

	// Calculate speed: length(platformMove) / platformMoveTime
	float speed = 0.0f;
	if (move.SquaredLength() > 0.0f && platformMoveTime > 0.0f) {
		speed = platformMove.Length() / platformMoveTime;
	}

	wp0.speed = speed;
	wp1.speed = speed;

	path->pathWaypoints.push_back(wp0);
	path->pathWaypoints.push_back(wp1);
	path->waypointCount = 2;

	m_GeneratedPath = std::move(path);
}
