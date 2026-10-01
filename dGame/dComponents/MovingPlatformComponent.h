/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MOVINGPLATFORMCOMPONENT_H
#define MOVINGPLATFORMCOMPONENT_H

#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include <string>

#include "dCommonVars.h"
#include "EntityManager.h"
#include "Component.h"
#include "eMovementPlatformState.h"
#include "eReplicaComponentType.h"

class Path;
namespace GameMessages { struct PlatformResync; }

 /**
  * Different types of available platforms
  */
enum class eMoverSubComponentType : uint32_t {
	mover = 4,

	/**
	 * A two point mover set up from the MovingPlatforms table or the platformMove* settings (used in NJ)
	 */
	simpleMover = 5,

	/**
	 * A mover that turns between rotations along its path (same data as a mover)
	 */
	rotater = 6,

	/**
	 * The level config set the platform flags all false: the client makes no subcomponent
	 */
	none = 0,
};

/**
 * The client's platform state flags (LWOPlatform)
 */
namespace ePlatformStateFlag {
	constexpr uint32_t Stopped = 1;
	constexpr uint32_t Travelling = 2;
	constexpr uint32_t Waiting = 4;
	constexpr uint32_t ReachedDesiredWaypoint = 8;
	constexpr uint32_t ReachedFinalDestination = 16;
};

/**
 * How the client picks the subcomponent (LWOMovingPlatformComponent::LoadConfigData): the platformIsRotater,
 * platformIsMover and platformIsSimpleMover settings, and when none is set a mover for a platform with no registry
 * component (set up from its attached path) or a simple mover for one with a registry component.
 * The client makes one subcomponent per flag set (mover, simple mover, rotater, LWOMovingPlatformComponent::SetupPlatform);
 * DLU keeps one, the first in that order.
 */
eMoverSubComponentType ChooseMoverSubComponentType(int32_t componentID, const Entity& entity);

/**
 * A mover between a start point and that point moved by platformMove (in the start rotation), taking
 * platformMoveTime seconds, as LWOPlatformSimpleMover::GenerateSimpleMoverPath builds its two waypoint path.
 */
class SimpleMoverSubComponent {
public:
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate);

	NiPoint3 mStartPosition{};
	NiQuaternion mStartRotation{};
	NiPoint3 mMove{};
	float mMoveTime = 0.0f;
	bool mStartAtEnd = false;

	/**
	 * ePlatformStateFlag bits. Live constructed simple movers stopped at their desired waypoint.
	 */
	uint32_t mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint;
	int32_t mCurrentWaypointIndex = 0;
	bool mInReverse = false;

	bool mDirtyStartingPoint = false;
	bool mDirtyState = false;

	/**
	 * The world position of a waypoint (0 the start, 1 the end)
	 */
	NiPoint3 GetWaypointPosition(int32_t index) const;
};

/**
 * Sub component for moving platforms that determine the actual current movement state
 */
/**
 * A mover (or rotater) along its path, run by the server as live ran it: the client's LWOPlatform logic (ArrivedAtWaypoint
 * 0x00c86c30, ProcessStateChange 0x00cce380, GetNextWaypoint 0x00bd4490 / GetNextReversedWaypoint 0x00bd4420, the speeds
 * of GetWaypointSpeed 0x00be90d0), with live's states: it travels (2), and at a waypoint it is stopped (1) with
 * ReachedDesiredWaypoint (8), and ReachedFinalDestination (16) at either end, while it waits the waypoint's wait time;
 * each arrival and departure is serialized (NT movers in the live captures: 25 on arrival, 2 on departure).
 */
class MoverSubComponent {
public:
	MoverSubComponent(const NiPoint3& startPos);
	~MoverSubComponent();

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate);

	// ePlatformStateFlag bits
	uint32_t mState = ePlatformStateFlag::Stopped | ePlatformStateFlag::ReachedDesiredWaypoint;
	int32_t mDesiredWaypointIndex = -1;
	bool mInReverse = false;
	bool mShouldStopAtDesiredWaypoint = false;
	// From the current waypoint (0) to the next (1)
	float mPercentBetweenPoints = 0;
	// Where it is now (current waypoint + percent towards the next)
	NiPoint3 mPosition{};
	uint32_t mCurrentWaypointIndex = 0;
	uint32_t mNextWaypointIndex = 0;
	// Time spent at the waypoint, and how long it waits there (the waypoint's wait, at least 0.1 s)
	float mIdleTimeElapsed = 0;
	float mTimeToWait = 0;
	// Whether it leaves when the wait is over (false: stopped until told otherwise)
	bool mDepartAfterWait = false;
	// Time travelled since the current waypoint (time based paths only)
	float mMoveTimeElapsed = 0;
	// The current and next waypoints' speeds; time based paths: the seconds from one to the next in mCurrentSpeed
	float mCurrentSpeed = 0;
	float mNextSpeed = 0;
	// From the current waypoint to the next
	float mSegmentLength = 0;
	bool mTimeBased = false;
};


/**
 * Represents entities that may be moving platforms, indicating how they should move through the world.
 * NOTE: the logic in this component hardly does anything, apparently the client can figure most of this stuff out
 * if you just serialize it correctly, resulting in smoother results anyway. Don't be surprised if the exposed APIs
 * don't at all do what you expect them to as we don't instruct the client of changes made here.
 * ^^^ Trivia: This made the red blocks platform and property platforms a pain to implement.
 */
class MovingPlatformComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MOVING_PLATFORM;

	MovingPlatformComponent(Entity* parent, const int32_t componentID, const std::string& pathName);

	void LoadDataFromTemplate() override;
	void LoadConfigData() override;
	~MovingPlatformComponent() override;

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Stops all pathing, called when an entity starts a quick build associated with this platform
	 */
	void OnQuickBuildInitilized();

	/**
	 * Starts the pathing, called when an entity completed a quick build associated with this platform
	 */
	void OnCompleteQuickBuild();

	/**
	 * Updates the movement state for the moving platform
	 * @param value the movement state to set
	 */
	void SetMovementState(eMovementPlatformState value);

	/**
	 * Instructs the moving platform to go to some waypoint
	 * @param index the index of the waypoint
	 * @param stopAtWaypoint determines if the platform should stop at the waypoint
	 */
	void GotoWaypoint(uint32_t index, bool stopAtWaypoint = true);

	/**
	 * Starts the pathing of this platform, setting appropriate waypoints and speeds
	 */
	void StartPathing();

	/**
	 * Stops the platform from moving, waiting for it to be activated again.
	 */
	void StopPathing();

	/**
	 * Determines if the entity should be serialized on the next update
	 * @param value whether to serialize the entity or not
	 */
	void SetSerialized(bool value);

	/**
	 * Returns if this platform will start automatically after spawn
	 * @return if this platform will start automatically after spawn
	 */
	bool GetNoAutoStart() const;

	/**
	 * Sets the auto start value for this platform
	 * @param value the auto start value to set
	 */
	void SetNoAutoStart(bool value);

	/**
	 * Warps the platform to a waypoint index, skipping its current path
	 * @param index the index to go to
	 */
	void WarpToWaypoint(size_t index);

	/**
	 * Returns the waypoint this platform was previously at
	 * @return the waypoint this platform was previously at
	 */
	size_t GetLastWaypointIndex() const;

	/**
	 * Returns the sub component that actually defines how the platform moves around (speeds, etc).
	 * @return the sub component that actually defines how the platform moves around
	 */
	MoverSubComponent* GetMoverSubComponent() const;

	/**
	 * Returns the simple mover sub component, if this platform is a simple mover
	 */
	SimpleMoverSubComponent* GetSimpleMoverSubComponent() const;

	eMoverSubComponentType GetMoverSubComponentType() const { return m_MoverSubComponentType; }

	/**
	 * Called when a simple mover gets to the waypoint it was sent to: the client's LWOPlatform::ArrivedAtWaypoint
	 * and HandleWaypointArrived (Arrived, then ArrivedAtDesiredWaypoint and PlatformAtLastWaypoint)
	 */
	void OnSimpleMoverArrived(int32_t index);

private:

	/**
	 * The path this platform is currently on
	 */
	const Path* m_Path = nullptr;

	/**
	 * The name of the path this platform is currently on
	 */
	std::u16string m_PathName;

	/**
	 * Whether the platform has stopped pathing
	 */
	bool m_PathingStopped = false;

	// The waypoint the path starts at (attached_path_start), written with the path
	uint32_t m_StartWaypoint = 0;

	/**
	 * The type of the subcomponent
	 */
	eMoverSubComponentType m_MoverSubComponentType;

	/**
	 * The mover sub component that belongs to this platform
	 */
	void* m_MoverSubComponent;

	/**
	 * Counts sends so an arrival timer from an older send does nothing
	 */
	uint32_t m_MoveGeneration = 0;

	void SimpleMoverGotoWaypoint(int32_t index);

	void FireArrivalHooks(uint32_t index, bool atDesiredWaypoint, bool atLastWaypoint);

	// The mover: index becomes the current waypoint (the next from the path's behavior and direction), at it
	void MoverProcessStateChange(uint32_t index);
	// At its current waypoint, stopped; it leaves after the waypoint's wait when depart
	void MoverStopAtWaypoint(bool depart);
	// Leaves (or carries on) towards the next waypoint
	void MoverDepart();
	void MoverArrive();

public:
	void Run(float deltaTime) override;

	// The mover's state as a PlatformResync gives it (RequestPlatformResync)
	void FillResync(GameMessages::PlatformResync& resync) const;

	// A mover on a path, run by the server (a rotater only spins: no waypoints)
	bool IsMover() const;

private:

	/**
	 * Whether the platform shouldn't auto start
	 */
	bool m_NoAutoStart;

	/**
	 * Whether to serialize the entity on the next update
	 */
	bool m_Serialize = false;
};

#endif // MOVINGPLATFORMCOMPONENT_H
