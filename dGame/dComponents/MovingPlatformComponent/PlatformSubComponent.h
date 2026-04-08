#ifndef PLATFORMSUBCOMPONENT_H
#define PLATFORMSUBCOMPONENT_H

#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"

#include <cstdint>

class Entity;
class Path;
class SimplePhysicsComponent;

/**
 * Platform state flags (bitmask matching client LWOPlatform state bits)
 */
namespace PlatformState {
	constexpr uint32_t Waiting = 1 << 0;    // 0x01 - Waiting at waypoint
	constexpr uint32_t Travelling = 1 << 1; // 0x02 - Moving between waypoints
	constexpr uint32_t Stopped = 1 << 2;    // 0x04 - Movement halted

	// These map to the old eMovementPlatformState values for serialization
	constexpr uint32_t MovingSerialized = 0b00010;      // Travelling
	constexpr uint32_t StationarySerialized = 0b11001;  // Waiting
	constexpr uint32_t StoppedSerialized = 0b01100;     // Stopped
};

/**
 * Base class for platform subcomponents. Mirrors the client's LWOPlatform base.
 * Handles the core state machine: waiting at waypoints, travelling between them,
 * arrival detection, and waypoint navigation (loop/bounce/once).
 */
class PlatformSubComponent {
public:
	PlatformSubComponent(Entity* parentEntity, const Path* path);
	virtual ~PlatformSubComponent() = default;

	virtual void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate);
	virtual void Update(float deltaTime, bool& dirtyOut);

	void StartPathing();
	void StopPathing();
	void GotoWaypoint(uint32_t index, bool stopAtWaypoint = true);
	void WarpToWaypoint(size_t index);
	void SetupWaypointSegment(uint32_t waypointIndex);

	// --- State accessors ---

	uint32_t GetState() const { return m_State; }
	void SetState(uint32_t state) { m_State = state; }

	int32_t GetDesiredWaypointIndex() const { return m_DesiredWaypointIndex; }
	bool GetInReverse() const { return m_InReverse; }
	bool GetShouldStopAtDesiredWaypoint() const { return m_ShouldStopAtDesiredWaypoint; }
	float GetPercentBetweenPoints() const { return m_PercentBetweenPoints; }
	NiPoint3 GetPosition() const { return m_Position; }
	uint32_t GetCurrentWaypointIndex() const { return m_CurrentWaypointIndex; }
	uint32_t GetNextWaypointIndex() const { return m_NextWaypointIndex; }
	float GetIdleTimeElapsed() const { return m_IdleTimeElapsed; }
	float GetMoveTimeElapsed() const { return m_MoveTimeElapsed; }
	float GetSpeed() const { return m_CurrentSpeed; }
	float GetWaitTime() const { return m_WaitTime; }
	size_t GetLastWaypointIndex() const;
	bool IsActive() const { return m_Active; }

	void SetDesiredWaypointIndex(int32_t index) { m_DesiredWaypointIndex = index; }
	void SetShouldStopAtDesiredWaypoint(bool value) { m_ShouldStopAtDesiredWaypoint = value; }
	void SetInReverse(bool value) { m_InReverse = value; }
	void SetActive(bool value) { m_Active = value; }

	uint32_t GetSerializedState() const;

protected:
	uint32_t GetNextWaypoint(uint32_t current, bool& changedDirection) const;
	uint32_t GetNextReversedWaypoint(uint32_t current, bool& changedDirection) const;
	bool CloseToNextWaypoint() const;

	static float CalculateAcceleration(float vi, float vf, float d);
	static float CalculateTime(float vi, float a, float d);
	void CalculateWaypointSpeeds();
	float CalculateCurrentSpeed() const;

	bool IncrementWaitingTime(float deltaTime);
	void StartTravelling();
	void ArrivedAtWaypoint(bool& dirtyOut);
	virtual void UpdatePositionAlongPath(float deltaTime);

	void SetPhysicsVelocity(const NiPoint3& velocity);
	void ZeroPhysicsVelocity();
	void PlayDepartSound();
	void PlayArriveSound();

	Entity* m_ParentEntity = nullptr;
	const Path* m_Path = nullptr;
	bool m_Active = false;

	uint32_t m_State = PlatformState::Stopped;
	int32_t m_DesiredWaypointIndex = -1;
	bool m_InReverse = false;
	bool m_ShouldStopAtDesiredWaypoint = false;

	float m_PercentBetweenPoints = 0.0f;
	NiPoint3 m_Position{};

	uint32_t m_CurrentWaypointIndex = 0;
	uint32_t m_NextWaypointIndex = 0;

	float m_IdleTimeElapsed = 0.0f;
	float m_MoveTimeElapsed = 0.0f;

	float m_CurrentSpeed = 0.0f;
	float m_NextSpeed = 0.0f;
	float m_WaitTime = 0.0f;

	NiPoint3 m_CurrentWaypointPosition{};
	NiPoint3 m_NextWaypointPosition{};
	NiQuaternion m_CurrentWaypointRotation = QuatUtils::IDENTITY;
	NiQuaternion m_NextWaypointRotation = QuatUtils::IDENTITY;
	NiPoint3 m_DirectionVector{};
	float m_TotalDistance = 0.0f;
	float m_TravelTime = 0.0f;

	bool m_TimeBasedMovement = false;
	bool m_HasStartedTravelling = false;
};

#endif // PLATFORMSUBCOMPONENT_H
