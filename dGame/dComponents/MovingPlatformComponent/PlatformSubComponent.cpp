#include "PlatformSubComponent.h"

#include "BitStream.h"
#include "BitStreamUtils.h"
#include "Entity.h"
#include "Game.h"
#include "dServer.h"
#include "GameMessages.h"
#include "CppScripts.h"
#include "SimplePhysicsComponent.h"
#include "Zone.h"
#include "MessageType/Client.h"
#include "MessageType/Game.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/quaternion.hpp>

PlatformSubComponent::PlatformSubComponent(Entity* parentEntity, const Path* path)
	: m_ParentEntity(parentEntity)
	, m_Path(path) {

	if (m_Path) {
		m_TimeBasedMovement = m_Path->movingPlatform.timeBasedMovement != 0;
	}

	m_Position = parentEntity ? parentEntity->GetPosition() : NiPoint3{};
}

void PlatformSubComponent::Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) {
	outBitStream.Write<bool>(true);

	outBitStream.Write<uint32_t>(GetSerializedState());
	outBitStream.Write<int32_t>(m_DesiredWaypointIndex);
	outBitStream.Write<bool>(m_ShouldStopAtDesiredWaypoint);
	outBitStream.Write<bool>(m_InReverse);

	outBitStream.Write<float>(m_PercentBetweenPoints);

	outBitStream.Write<float>(m_Position.x);
	outBitStream.Write<float>(m_Position.y);
	outBitStream.Write<float>(m_Position.z);

	outBitStream.Write<uint32_t>(m_CurrentWaypointIndex);
	outBitStream.Write<uint32_t>(m_NextWaypointIndex);

	outBitStream.Write<float>(m_IdleTimeElapsed);
	outBitStream.Write<float>(m_MoveTimeElapsed);
}

uint32_t PlatformSubComponent::GetSerializedState() const {
	if (m_State & PlatformState::Stopped) return PlatformState::StoppedSerialized;
	if (m_State & PlatformState::Travelling) return PlatformState::MovingSerialized;
	return PlatformState::StationarySerialized;
}

void PlatformSubComponent::Update(float deltaTime, bool& dirtyOut) {
	if (!m_Active) return;
	if (m_State == 0) return;
	if (!m_Path || m_Path->pathWaypoints.empty()) return;

	if (IncrementWaitingTime(deltaTime)) {
		StartTravelling();
		dirtyOut = true;
	}

	if (m_State & PlatformState::Travelling) {
		UpdatePositionAlongPath(deltaTime);

		bool arrived = false;
		if (m_TimeBasedMovement) {
			arrived = m_TravelTime > 0.0f && std::abs(m_TravelTime - m_MoveTimeElapsed) < 0.001f;
		} else {
			arrived = CloseToNextWaypoint();
		}

		if (arrived) {
			ArrivedAtWaypoint(dirtyOut);
		}
	}
}

// --- Movement control ---

void PlatformSubComponent::StartPathing() {
	if (!m_Path || m_Path->pathWaypoints.empty()) return;

	m_Active = true;
	SetupWaypointSegment(m_CurrentWaypointIndex);

	m_State = PlatformState::Waiting | PlatformState::Stopped;
	m_IdleTimeElapsed = 0.0f;
	m_MoveTimeElapsed = 0.0f;
	m_PercentBetweenPoints = 0.0f;
	m_HasStartedTravelling = false;
}

void PlatformSubComponent::StopPathing() {
	m_State = PlatformState::Stopped;
	m_DesiredWaypointIndex = -1;
	m_ShouldStopAtDesiredWaypoint = false;
	m_MoveTimeElapsed = 0.0f;
	m_HasStartedTravelling = false;

	ZeroPhysicsVelocity();
}

void PlatformSubComponent::GotoWaypoint(uint32_t index, bool stopAtWaypoint) {
	m_DesiredWaypointIndex = static_cast<int32_t>(index);
	m_NextWaypointIndex = index;
	m_ShouldStopAtDesiredWaypoint = stopAtWaypoint;

	StartPathing();
}

void PlatformSubComponent::WarpToWaypoint(size_t index) {
	if (!m_Path || index >= m_Path->pathWaypoints.size()) return;

	const auto& waypoint = m_Path->pathWaypoints[index];
	m_Position = waypoint.position;
	m_CurrentWaypointIndex = static_cast<uint32_t>(index);
	m_PercentBetweenPoints = 0.0f;
	m_MoveTimeElapsed = 0.0f;

	if (m_ParentEntity) {
		m_ParentEntity->SetPosition(waypoint.position);
		m_ParentEntity->SetRotation(waypoint.rotation);
	}
}

size_t PlatformSubComponent::GetLastWaypointIndex() const {
	if (!m_Path || m_Path->pathWaypoints.empty()) return 0;
	return m_Path->pathWaypoints.size() - 1;
}

// --- Waypoint segment setup (mirrors client ProcessStateChange) ---

void PlatformSubComponent::SetupWaypointSegment(uint32_t waypointIndex) {
	if (!m_Path || m_Path->pathWaypoints.empty()) return;

	m_CurrentWaypointIndex = waypointIndex;

	const auto& currentWP = m_Path->pathWaypoints[m_CurrentWaypointIndex];
	m_CurrentWaypointPosition = currentWP.position;
	m_CurrentWaypointRotation = currentWP.rotation;
	m_WaitTime = currentWP.movingPlatform.wait;
	m_Position = currentWP.position;

	bool changedDirection = false;
	if (!m_InReverse) {
		m_NextWaypointIndex = GetNextWaypoint(m_CurrentWaypointIndex, changedDirection);
		if (changedDirection) m_InReverse = true;
	} else {
		m_NextWaypointIndex = GetNextReversedWaypoint(m_CurrentWaypointIndex, changedDirection);
		if (changedDirection) m_InReverse = false;
	}

	const auto& nextWP = m_Path->pathWaypoints[m_NextWaypointIndex];
	m_NextWaypointPosition = nextWP.position;
	m_NextWaypointRotation = nextWP.rotation;

	m_DirectionVector = m_NextWaypointPosition - m_CurrentWaypointPosition;
	m_TotalDistance = m_DirectionVector.Length();
	if (m_TotalDistance > 0.0f) {
		m_DirectionVector = m_DirectionVector / m_TotalDistance;
	}

	CalculateWaypointSpeeds();

	m_MoveTimeElapsed = 0.0f;
	m_IdleTimeElapsed = 0.0f;
	m_HasStartedTravelling = false;

	if (m_TimeBasedMovement) {
		m_PercentBetweenPoints = 0.0f;
	} else if (m_TotalDistance > 0.0f) {
		m_PercentBetweenPoints = (m_Position - m_CurrentWaypointPosition).Length() / m_TotalDistance;
	} else {
		m_PercentBetweenPoints = 0.0f;
	}

	if (m_ParentEntity) {
		m_ParentEntity->SetPosition(m_CurrentWaypointPosition);
		m_ParentEntity->SetRotation(m_CurrentWaypointRotation);
	}
}

// --- Waypoint navigation (exact match of client decompilation) ---

uint32_t PlatformSubComponent::GetNextWaypoint(uint32_t current, bool& changedDirection) const {
	changedDirection = false;
	uint32_t next = current + 1;
	const auto numWaypoints = static_cast<uint32_t>(m_Path->pathWaypoints.size());

	if (next >= numWaypoints) {
		switch (m_Path->pathBehavior) {
		case PathBehavior::Once:
			next = numWaypoints - 1;
			break;
		case PathBehavior::Bounce:
			next = numWaypoints >= 2 ? numWaypoints - 2 : 0;
			changedDirection = true;
			break;
		case PathBehavior::Loop:
		default:
			next = 0;
			break;
		}
	}

	return next;
}

uint32_t PlatformSubComponent::GetNextReversedWaypoint(uint32_t current, bool& changedDirection) const {
	changedDirection = false;

	if (current == 0) {
		switch (m_Path->pathBehavior) {
		case PathBehavior::Once:
			return 0;
		case PathBehavior::Bounce:
			changedDirection = true;
			return 1;
		case PathBehavior::Loop:
		default:
			return static_cast<uint32_t>(m_Path->pathWaypoints.size()) - 1;
		}
	}

	return current - 1;
}

// --- Arrival detection ---

bool PlatformSubComponent::CloseToNextWaypoint() const {
	if (m_TimeBasedMovement) return false;

	const NiPoint3 toNext = m_NextWaypointPosition - m_Position;
	const float distSq = toNext.SquaredLength();

	if (distSq <= 0.001f) return true;

	const float dot = toNext.DotProduct(m_DirectionVector);
	return dot <= 0.0f;
}

// --- Travel time calculation ---

float PlatformSubComponent::CalculateAcceleration(float vi, float vf, float d) {
	if (d < 0.0001f) return 0.0f;
	return (vf * vf - vi * vi) / (2.0f * d);
}

float PlatformSubComponent::CalculateTime(float vi, float a, float d) {
	if (d < 0.0001f) return 0.0f;
	if (std::abs(a) < 0.0001f) {
		return vi > 0.0f ? d / vi : 0.0f;
	}
	const float discriminant = 2.0f * a * d + vi * vi;
	if (discriminant < 0.0f) return 0.0f;
	return (std::sqrt(discriminant) - vi) / a;
}

void PlatformSubComponent::CalculateWaypointSpeeds() {
	if (m_CurrentWaypointIndex == m_NextWaypointIndex) {
		m_TravelTime = 0.0f;
		return;
	}

	if (m_TimeBasedMovement) {
		uint32_t minIdx = std::min(m_CurrentWaypointIndex, m_NextWaypointIndex);
		m_CurrentSpeed = m_Path->pathWaypoints[minIdx].speed;
		m_NextSpeed = 0.0f;
		m_TravelTime = m_CurrentSpeed;
	} else {
		m_CurrentSpeed = m_Path->pathWaypoints[m_CurrentWaypointIndex].speed;
		m_NextSpeed = m_Path->pathWaypoints[m_NextWaypointIndex].speed;

		float a = CalculateAcceleration(m_CurrentSpeed, m_NextSpeed, m_TotalDistance);
		m_TravelTime = CalculateTime(m_CurrentSpeed, a, m_TotalDistance);
	}
}

float PlatformSubComponent::CalculateCurrentSpeed() const {
	if (m_TimeBasedMovement) {
		if (m_CurrentSpeed > 0.0f) {
			return m_TotalDistance / m_CurrentSpeed;
		}
		return 0.0f;
	}

	return (m_NextSpeed - m_CurrentSpeed) * m_PercentBetweenPoints + m_CurrentSpeed;
}

// --- State machine helpers ---

bool PlatformSubComponent::IncrementWaitingTime(float deltaTime) {
	if (!(m_State & PlatformState::Waiting)) return false;
	if (m_State & PlatformState::Travelling) return false;

	m_IdleTimeElapsed += deltaTime;
	if (m_IdleTimeElapsed >= m_WaitTime) {
		m_IdleTimeElapsed = 0.0f;
		return true;
	}
	return false;
}

void PlatformSubComponent::StartTravelling() {
	m_State = (m_State & ~(PlatformState::Stopped | PlatformState::Waiting)) | PlatformState::Travelling;
	m_MoveTimeElapsed = 0.0f;
	m_HasStartedTravelling = false;
}

void PlatformSubComponent::ArrivedAtWaypoint(bool& dirtyOut) {
	dirtyOut = true;

	m_Position = m_NextWaypointPosition;
	m_PercentBetweenPoints = 1.0f;

	if (m_ParentEntity) {
		m_ParentEntity->SetPosition(m_NextWaypointPosition);
		m_ParentEntity->SetRotation(m_NextWaypointRotation);
	}

	PlayArriveSound();

	if (m_ParentEntity) {
		m_ParentEntity->GetScript()->OnWaypointReached(m_ParentEntity, m_NextWaypointIndex);
	}

	bool isAtDesiredWaypoint = false;
	bool stopAtDesired = false;
	if (m_DesiredWaypointIndex >= 0 &&
		static_cast<uint32_t>(m_DesiredWaypointIndex) == m_NextWaypointIndex) {
		isAtDesiredWaypoint = true;
		stopAtDesired = m_ShouldStopAtDesiredWaypoint;
		m_ShouldStopAtDesiredWaypoint = false;
		m_DesiredWaypointIndex = -1;
	}

	if (isAtDesiredWaypoint && m_ParentEntity) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(m_ParentEntity->GetObjectID());
		bitStream.Write(MessageType::Game::ARRIVED_AT_DESIRED_WAYPOINT);
		SEND_PACKET_BROADCAST;
	}

	bool atEnd = false;
	const auto numWaypoints = static_cast<uint32_t>(m_Path->pathWaypoints.size());
	if (m_NextWaypointIndex == 0 || m_NextWaypointIndex == numWaypoints - 1) {
		atEnd = true;
	}

	if (atEnd && m_ParentEntity) {
		CBITSTREAM;
		CMSGHEADER;
		bitStream.Write(m_ParentEntity->GetObjectID());
		bitStream.Write(MessageType::Game::PLATFORM_AT_LAST_WAYPOINT);
		SEND_PACKET_BROADCAST;
	}

	bool stopOnce = false;
	if (atEnd && m_Path->pathBehavior == PathBehavior::Once) {
		stopOnce = true;
		m_InReverse = !m_InReverse;
	}

	if (stopAtDesired || stopOnce) {
		m_State = PlatformState::Stopped;
		m_MoveTimeElapsed = 0.0f;
		m_HasStartedTravelling = false;
		ZeroPhysicsVelocity();

		if (m_ParentEntity) {
			CBITSTREAM;
			CMSGHEADER;
			bitStream.Write(m_ParentEntity->GetObjectID());
			bitStream.Write(MessageType::Game::ARRIVED);
			SEND_PACKET_BROADCAST;
		}
	} else {
		SetupWaypointSegment(m_NextWaypointIndex);
		m_State = PlatformState::Waiting;
	}
}

void PlatformSubComponent::UpdatePositionAlongPath(float deltaTime) {
	if (m_TotalDistance <= 0.0f && !m_TimeBasedMovement) return;

	m_MoveTimeElapsed += deltaTime;

	// Calculate percent between waypoints matching client CalculatePercentTravelledToWaypoint:
	// Distance-based: percent = dist(position, currentWP) / dist(nextWP, currentWP)
	// Time-based: percent = moveTimeElapsed / travelTime
	if (m_TimeBasedMovement) {
		if (m_TravelTime > 0.0f) {
			m_MoveTimeElapsed = std::min(m_MoveTimeElapsed, m_TravelTime);
			m_PercentBetweenPoints = m_MoveTimeElapsed / m_TravelTime;
		}
	} else if (m_TotalDistance > 0.0f) {
		float distanceTravelled = (m_Position - m_CurrentWaypointPosition).Length();
		m_PercentBetweenPoints = std::min(distanceTravelled / m_TotalDistance, 1.0f);
	}

	// Send Departed message on first travel frame (matching client RunPlatform)
	if (!m_HasStartedTravelling) {
		m_HasStartedTravelling = true;
		PlayDepartSound();

		if (m_ParentEntity) {
			CBITSTREAM;
			CMSGHEADER;
			bitStream.Write(m_ParentEntity->GetObjectID());
			bitStream.Write(MessageType::Game::DEPARTED);
			SEND_PACKET_BROADCAST;
		}
	}

	// Advance position using velocity and deltaTime (matching client physics model)
	// The client sets velocity then lets the physics engine move the object.
	// We do the same: calculate speed, derive velocity, advance position.
	float speed = CalculateCurrentSpeed();
	NiPoint3 velocity = m_DirectionVector * speed;
	m_Position = m_Position + velocity * deltaTime;

	// Clamp position to not overshoot the next waypoint
	float distToNext = (m_NextWaypointPosition - m_Position).DotProduct(m_DirectionVector);
	if (distToNext <= 0.0f) {
		m_Position = m_NextWaypointPosition;
	}

	if (m_ParentEntity) {
		m_ParentEntity->SetPosition(m_Position);
		SetPhysicsVelocity(velocity);

		// Slerp rotation between waypoints
		auto interpRot = glm::slerp(m_CurrentWaypointRotation, m_NextWaypointRotation, m_PercentBetweenPoints);
		m_ParentEntity->SetRotation(interpRot);
	}
}

// --- Physics velocity helpers ---

void PlatformSubComponent::SetPhysicsVelocity(const NiPoint3& velocity) {
	if (!m_ParentEntity) return;
	auto* simplePhysics = m_ParentEntity->GetComponent<SimplePhysicsComponent>();
	if (simplePhysics) {
		simplePhysics->SetVelocity(velocity);
	}
}

void PlatformSubComponent::ZeroPhysicsVelocity() {
	if (!m_ParentEntity) return;
	auto* simplePhysics = m_ParentEntity->GetComponent<SimplePhysicsComponent>();
	if (simplePhysics) {
		simplePhysics->SetVelocity(NiPoint3Constant::ZERO);
		simplePhysics->SetAngularVelocity(NiPoint3Constant::ZERO);
	}
}

// --- Sound helpers ---

void PlatformSubComponent::PlayDepartSound() {
	if (!m_ParentEntity || !m_Path) return;
	if (m_CurrentWaypointIndex >= m_Path->pathWaypoints.size()) return;

	const auto& sound = m_Path->pathWaypoints[m_CurrentWaypointIndex].movingPlatform.departSound;
	if (!sound.empty()) {
		GameMessages::SendPlayNDAudioEmitter(m_ParentEntity, UNASSIGNED_SYSTEM_ADDRESS, sound);
	}
}

void PlatformSubComponent::PlayArriveSound() {
	if (!m_ParentEntity || !m_Path) return;
	if (m_NextWaypointIndex >= m_Path->pathWaypoints.size()) return;

	const auto& sound = m_Path->pathWaypoints[m_NextWaypointIndex].movingPlatform.arriveSound;
	if (!sound.empty()) {
		GameMessages::SendPlayNDAudioEmitter(m_ParentEntity, UNASSIGNED_SYSTEM_ADDRESS, sound);
	}
}
