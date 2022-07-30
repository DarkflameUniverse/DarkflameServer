/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MOVINGPLATFORMCOMPONENT_H
#define MOVINGPLATFORMCOMPONENT_H

#include "RakNetTypes.h"
#include "NiPoint3.h"
#include <string>

#include "dCommonVars.h"
#include "EntityManager.h"
#include "Component.h"

 /**
  * Different types of available platforms
  */
enum class eMoverSubComponentType : uint32_t {
	mover = 4,

	/**
	 * Used in NJ
	 */
	 simpleMover = 5,
};

/**
 * The different types of platform movement state, supposedly a bitmap
 */
enum class MovementPlatformState : uint32_t
{
	Moving = 0b00010,
	Stationary = 0b11001,
	Stopped = 0b01100
};

/**
 * Sub component for moving platforms that determine the actual current movement state
 */
class MoverSubComponent {
public:
	MoverSubComponent(const NiPoint3& startPos);
	~MoverSubComponent();

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) const;

	/**
	 * The state the platform is currently in
	 */
	MovementPlatformState mState = MovementPlatformState::Stationary;

	/**
	 * The waypoint this platform currently wants to traverse to
	 */
	int32_t mDesiredWaypointIndex = 0;

	/**
	 * Whether the platform is currently reversing away from the desired waypoint
	 */
	bool mInReverse = false;

	/**
	 * Whether the platform should stop moving when reaching the desired waypoint
	 */
	bool mShouldStopAtDesiredWaypoint = false;

	/**
	 * The percentage of the way between the last point and the desired point
	 */
	float mPercentBetweenPoints = 0;

	/**
	 * The current position of the platofrm
	 */
	NiPoint3 mPosition{};

	/**
	 * The waypoint the platform is (was) at
	 */
	uint32_t mCurrentWaypointIndex;

	/**
	 * The waypoint the platform is attempting to go to
	 */
	uint32_t mNextWaypointIndex;

	/**
	 * The timer that handles the time before stopping idling and continue platform movement
	 */
	float mIdleTimeElapsed = 0;

	/**
	 * The speed the platform is currently moving at
	 */
	float mSpeed = 0;

	/**
	 * The time to wait before continuing movement
	 */
	float mWaitTime = 0;
};


/**
 * Represents entities that may be moving platforms, indicating how they should move through the world.
 * NOTE: the logic in this component hardly does anything, apparently the client can figure most of this stuff out
 * if you just serialize it correctly, resulting in smoother results anyway. Don't be surprised if the exposed APIs
 * don't at all do what you expect them to as we don't instruct the client of changes made here.
 * ^^^ Trivia: This made the red blocks platform and property platforms a pain to implement.
 */
class MovingPlatformComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_MOVING_PLATFORM;

	MovingPlatformComponent(Entity* parent, const std::string& pathName);
	~MovingPlatformComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

	/**
	 * Stops all pathing, called when an entity starts a quick build associated with this platform
	 */
	void OnRebuildInitilized();

	/**
	 * Starts the pathing, called when an entity completed a quick build associated with this platform
	 */
	void OnCompleteRebuild();

	/**
	 * Updates the movement state for the moving platform
	 * @param value the movement state to set
	 */
	void SetMovementState(MovementPlatformState value);

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
	 * Continues the path of the platform, after it's been stopped
	 */
	void ContinuePathing();

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

	/**
	 * The type of the subcomponent
	 */
	eMoverSubComponentType m_MoverSubComponentType;

	/**
	 * The mover sub component that belongs to this platform
	 */
	void* m_MoverSubComponent;

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
