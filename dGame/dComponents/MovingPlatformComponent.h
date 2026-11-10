/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MOVINGPLATFORMCOMPONENT_H
#define MOVINGPLATFORMCOMPONENT_H

#include <functional>
#include <memory>
#include <vector>

#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include <string>

#include "dCommonVars.h"
#include "EntityManager.h"
#include "Component.h"
#include "eMovementPlatformState.h"
#include "eReplicaComponentType.h"

struct ObjectLoadState;

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
 * The client's platform state flags (LWOPlatform), as live sent them: a platform that was never started is stopped at
 * its waypoint (12), one waiting out a waypoint's wait is 9 (25 at either end of its path), one on its way is 2 and
 * one that stopped at an end of a path run once is 28.
 */
namespace ePlatformStateFlag {
	constexpr uint32_t Waiting = 1;
	constexpr uint32_t Travelling = 2;
	constexpr uint32_t Stopped = 4;
	constexpr uint32_t ReachedDesiredWaypoint = 8;
	constexpr uint32_t ReachedFinalDestination = 16;
	// What LWOPlatform::UpdateVelocities 0x00bd4330 keeps when the platform sets off (it drops the two "reached" bits)
	constexpr uint32_t MovementMask = Waiting | Travelling | Stopped;
};

/**
 * How the client picks the subcomponents (LWOMovingPlatformComponent::LoadConfigData and SetupPlatform): one for each
 * of the platformIsMover, platformIsSimpleMover and platformIsRotater settings that is true, in that order, and when
 * none is set a mover for a platform with no registry component (set up from its attached path) or a simple mover for
 * one with a registry component.
 */
std::vector<eMoverSubComponentType> ChooseSubComponentTypes(int32_t componentID, const Entity& entity);

// The first of ChooseSubComponentTypes (none when there are none): the one the server runs
eMoverSubComponentType ChooseMoverSubComponentType(int32_t componentID, const Entity& entity);

/**
 * The base of the platform subcomponents, as the client's LWOPlatform (mover, simple mover and rotator are built on
 * it): the platform's state between the waypoints of its path, which LWOPlatform::Deserialize 0x00c870e0 reads.
 */
class PlatformSubComponent {
public:
	virtual ~PlatformSubComponent() = default;

	virtual eMoverSubComponentType GetType() const = 0;

	// LWOPlatform::Deserialize 0x00c870e0: a dirty bit, the state, desired waypoint, whether to stop there, reverse, the
	// percent between the waypoints, where it is, the current and next waypoints, the time waited and the time travelled
	virtual void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate);

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
	// Time spent at the waypoint, and how long it waits there (LWOPlatform::SetWaitingTime 0x00bdc860: the waypoint's
	// wait, at least 0.1 s); the wait only counts once it was set
	float mIdleTimeElapsed = 0;
	float mTimeToWait = 0;
	bool mWaitSet = false;
	// The current waypoint's wait
	float mWaitTime = 0;
	// Whether it runs: a quick build's platform does not until it is built, nor while the quick build is open again
	// (LWOPlatform::SendMessage 0x00bdc930, RebuildNotifyState)
	bool mActive = true;
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
 * A mover along its path (4), as the client's LWOPlatformMover, run by the server as live ran it: the client's
 * LWOPlatform logic (RunPlatform 0x00c86e10, ArrivedAtWaypoint 0x00c86c30, HandleWaypointArrived 0x00c17bc0,
 * ResumeTravelling 0x00c86b90, ProcessStateChange 0x00cce380, GetNextWaypoint 0x00bd4490 / GetNextReversedWaypoint
 * 0x00bd4420, the speeds of GetWaypointSpeed 0x00be90d0). Live serialized it when it began waiting at a waypoint and
 * when it left one, not when it stopped (the client stops by itself).
 */
class MoverSubComponent final : public PlatformSubComponent {
public:
	MoverSubComponent(const NiPoint3& startPos) { mPosition = startPos; }
	eMoverSubComponentType GetType() const override { return eMoverSubComponentType::mover; }
};

/**
 * A rotator (6), as the client's LWOPlatformRotator: it only spins (rotX/rotY/rotZ at rate) while travelling; it sends
 * the LWOPlatform data a mover sends.
 */
class RotatorSubComponent final : public PlatformSubComponent {
public:
	RotatorSubComponent(const NiPoint3& startPos) { mPosition = startPos; }
	eMoverSubComponentType GetType() const override { return eMoverSubComponentType::rotater; }
};

/**
 * A simple mover (5), as the client's LWOPlatformSimpleMover: between a start point and that point moved by
 * platformMove (in the start rotation), taking platformMoveTime seconds, as GenerateSimpleMoverPath builds its two
 * waypoint path. At an end it has stopped there (28), as any platform on a path run once.
 */
class SimpleMoverSubComponent final : public PlatformSubComponent {
public:
	// Live constructed the NJ counterweights waiting at their waypoint (9)
	SimpleMoverSubComponent() { mState = ePlatformStateFlag::Waiting | ePlatformStateFlag::ReachedDesiredWaypoint; }
	eMoverSubComponentType GetType() const override { return eMoverSubComponentType::simpleMover; }

	// LWOPlatformSimpleMover::Deserialize: a dirty bit, then whether there is a starting point with its position and
	// rotation (w, x, y, z); a dirty bit, then the state, the current waypoint and whether it is reversing
	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	NiPoint3 mStartPosition{};
	NiQuaternion mStartRotation{};
	NiPoint3 mMove{};
	float mMoveTime = 0.0f;
	bool mStartAtEnd = false;

	bool mDirtyStartingPoint = false;
	bool mDirtyState = false;

	/**
	 * The world position of a waypoint (0 the start, 1 the end)
	 */
	NiPoint3 GetWaypointPosition(int32_t index) const;
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

	// The client's allocator for this type (ObjectLoader2::DoObjectComponentLoad calls it in load order)
	static void Allocator(Entity& entity, ObjectLoadState& state);

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
	 * Puts the platform on a path of the zone, at one of its waypoints (the SetCurrentPath game message): a mover
	 * starts over on it. False when the platform is not a mover or the zone has no such path.
	 */
	bool SetCurrentPath(const std::string& pathName, uint32_t startPoint = 0, bool reverse = false);

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
	 * The mover or rotator the server runs (the first subcomponent), if it is one
	 */
	PlatformSubComponent* GetMoverSubComponent() const;

	/**
	 * Returns the simple mover sub component, if this platform is a simple mover
	 */
	SimpleMoverSubComponent* GetSimpleMoverSubComponent() const;

	eMoverSubComponentType GetMoverSubComponentType() const { return m_MoverSubComponentType; }

	/**
	 * Calls back whenever the platform is at the last waypoint of its path (the PlatformAtLastWaypoint game message,
	 * which live's scripts ask to be notified of on the NJ counterweights)
	 */
	void AddPlatformAtLastWaypointCallback(const std::function<void()>& callback);

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
	// Whether it runs the path backwards from there (SetCurrentPath), written with the path
	bool m_PathReverse = false;
	// The path changed since it was last written
	bool m_DirtyPath = false;

	/**
	 * The type of the first subcomponent, the one the server runs (none without one)
	 */
	eMoverSubComponentType m_MoverSubComponentType = eMoverSubComponentType::none;

	/**
	 * The subcomponents, one per type the client makes (ChooseSubComponentTypes), in its order
	 */
	std::vector<std::unique_ptr<PlatformSubComponent>> m_SubComponents;

	std::vector<std::function<void()>> m_AtLastWaypointCallbacks;

	// The object's quick build, looked up when the platform first runs
	class QuickBuildComponent* m_QuickBuild = nullptr;
	bool m_CheckedQuickBuild = false;

	/**
	 * Counts sends so an arrival timer from an older send does nothing
	 */
	uint32_t m_MoveGeneration = 0;

	void SimpleMoverGotoWaypoint(int32_t index);

	void FireArrivalHooks(uint32_t index, bool atDesiredWaypoint, bool atLastWaypoint);

	// LWOPlatform::ProcessStateChange: index becomes the current waypoint (the next from the path's behavior and direction)
	void MoverProcessStateChange(uint32_t index);
	// The mover at its starting waypoint on the path, started unless startPathingOnLoad is false
	void SetupMoverPath();
	// Puts the object where the mover is, without a serialization of its physics
	void MoveObject();
	// LWOPlatform::SetWaitingTime: starts a wait of at least 0.1 s
	void MoverSetWaitingTime(float waitTime, float idleTimeElapsed);
	// LWOPlatform::ResumeTravelling: stopped between two waypoints it travels on; at a waypoint it first waits there
	void MoverResumeTravelling();
	// LWOPlatform::ArrivedAtWaypoint: once it is at its next waypoint
	void MoverArrivedAtWaypoint();
	// LWOPlatform::HandleWaypointArrived: tells the scripts, then a platform that travels on waits at the waypoint
	void MoverHandleWaypointArrived(uint32_t index, bool atDesiredWaypoint, bool atLastWaypoint);

public:
	void Run(float deltaTime) override;

	// The mover's state as a PlatformResync gives it (RequestPlatformResync)
	void FillResync(GameMessages::PlatformResync& resync) const;

	// A mover on a path, run by the server (a rotater only spins: no waypoints)
	bool IsMover() const;

	// Whether the platform follows a path (its attached_path); a simple mover without one has no waypoints
	bool HasPath() const { return m_Path != nullptr; }

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
