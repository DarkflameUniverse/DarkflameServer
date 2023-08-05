/*
 * Darkflame Universe
 * Copyright 2019
 */

#ifndef MOVINGPLATFORMCOMPONENT_H
#define MOVINGPLATFORMCOMPONENT_H

#include "NiPoint3.h"
#include <memory>
#include <string>
#include <vector>

#include "dCommonVars.h"
#include "Component.h"
#include "eReplicaComponentType.h"

class PathWaypoint;
class Path;

/**
 * The different types of platform movement state
 */
enum eMovementPlatformState : uint32_t
{
	Waiting = 1 << 0U,
	Travelling = 1 << 1U,
	Stopped = 1 << 2U,
	ReachedDesiredWaypoint = 1 << 3U,
	ReachedFinalWaypoint = 1 << 4U,
};

/**
 * Different types of available platforms
 */
enum class eMoverSubComponentType : uint32_t {
	None = 0,
	Mover = 4,
	SimpleMover = 5,
	Rotator = 6
};

class MovingPlatformComponent;

// In the context of a platform that is TimeBasedMovement, 
// the speed member from the Path is used as the time to go between waypoints.
class PlatformSubComponent {
public:
	PlatformSubComponent(MovingPlatformComponent* parentComponent);
	virtual ~PlatformSubComponent() = default;
	virtual void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate);
	virtual eMoverSubComponentType GetPlatformType() { return eMoverSubComponentType::None; };
	bool GetIsDirty() const { return m_IsDirty; }
	virtual void LoadDataFromTemplate() {};
	virtual void LoadConfigData() {};
	virtual void StartPathing();
	virtual void ResumePathing();
	virtual void StopPathing();
	virtual void Update(float deltaTime);
	float CalculateSpeed() const;
	const PathWaypoint& GetNextWaypoint() const;
	const PathWaypoint& GetCurrentWaypoint() const;
	void SetupPath(const std::string& pathName, uint32_t startingWaypointIndex, bool startsInReverse);
	void AdvanceToNextWaypoint();
	void AdvanceToNextReverseWaypoint();
	NiPoint3 CalculateLinearVelocity();
	void UpdateLinearVelocity();
	void UpdateAngularVelocity();
	float CalculatePercentToNextWaypoint();

	// Write all the getters for the below members
	bool GetTimeBasedMovement() const { return m_TimeBasedMovement; }
	const Path* GetPath() const { return m_Path; }
	float GetSpeed() const { return m_Speed; }
	float GetWaitTime() const { return m_WaitTime; }
	float GetMoveTimeElapsed() const { return m_MoveTimeElapsed; }
	float GetPercentUntilNextWaypoint() const { return m_PercentUntilNextWaypoint; }
	int32_t GetCurrentWaypointIndex() const { return m_CurrentWaypointIndex; }
	int32_t GetNextWaypointIndex() const { return m_NextWaypointIndex; }
	bool GetInReverse() const { return m_InReverse; }
	bool GetShouldStopAtDesiredWaypoint() const { return m_ShouldStopAtDesiredWaypoint; }
	int32_t GetDesiredWaypointIndex() const { return m_DesiredWaypointIndex; }
	uint32_t GetState() const { return m_State; }
	const NiPoint3& GetPosition() const { return m_Position; }
	const NiQuaternion& GetRotation() const { return m_Rotation; }
	const NiPoint3& GetLinearVelocity() const { return m_LinearVelocity; }
	const NiPoint3& GetAngularVelocity() const { return m_AngularVelocity; }
	const MovingPlatformComponent* GetParentComponent() const { return m_ParentComponent; }
	const float GetIdleTimeElapsed() const { return m_IdleTimeElapsed; }


protected:
	MovingPlatformComponent* m_ParentComponent;

	/**
	 * The state the platform is currently in
	 */
	uint32_t m_State;
	int32_t m_DesiredWaypointIndex;
	float m_PercentUntilNextWaypoint;
	NiPoint3 m_Position;
	int32_t m_CurrentWaypointIndex;
	int32_t m_NextWaypointIndex;
	float m_IdleTimeElapsed;
	float m_Speed;
	float m_WaitTime;
	float m_MoveTimeElapsed;
	bool m_IsDirty;
	bool m_InReverse;
	bool m_ShouldStopAtDesiredWaypoint;
	NiPoint3 m_LinearVelocity;
	NiPoint3 m_AngularVelocity;
	bool m_TimeBasedMovement;
	const Path* m_Path;
	NiQuaternion m_Rotation;
};

class MoverPlatformSubComponent : public PlatformSubComponent {
public:
	inline static const eMoverSubComponentType SubComponentType = eMoverSubComponentType::Mover;
	MoverPlatformSubComponent(MovingPlatformComponent* parentComponent);
	~MoverPlatformSubComponent() override = default;
	eMoverSubComponentType GetPlatformType() override { return eMoverSubComponentType::Mover; }
};

class RotatorPlatformSubComponent : public PlatformSubComponent {
public:
	inline static const eMoverSubComponentType SubComponentType = eMoverSubComponentType::Rotator;
	RotatorPlatformSubComponent(MovingPlatformComponent* parentComponent);
	~RotatorPlatformSubComponent() override = default;
	eMoverSubComponentType GetPlatformType() override { return eMoverSubComponentType::Rotator; }
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override { PlatformSubComponent::Serialize(outBitStream, bIsInitialUpdate); };
};

// Only moves. Has NO path. This moving platform gets its initial position and rotation from the server on serialization.
class SimpleMoverPlatformSubComponent : public PlatformSubComponent {
public:
	inline static const eMoverSubComponentType SubComponentType = eMoverSubComponentType::SimpleMover;
	SimpleMoverPlatformSubComponent(MovingPlatformComponent* parentComponent, const NiPoint3& platformMove, const bool startAtEnd);
	~SimpleMoverPlatformSubComponent() override = default;
	eMoverSubComponentType GetPlatformType() override { return eMoverSubComponentType::SimpleMover; }
	void LoadConfigData() override;
	void LoadDataFromTemplate() override;
	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;
	bool m_HasStartingPoint = false;
	bool m_DirtyStartingPoint = false;
	NiPoint3 m_StartingPoint;
	NiQuaternion m_StartingRotation;
	NiPoint3 m_PlatformMove;
	float m_MoveTime;
	bool m_StartAtEnd;
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
	static const eReplicaComponentType ComponentType = eReplicaComponentType::MOVING_PLATFORM;

	MovingPlatformComponent(Entity* parent, const std::string& pathName);

	void LoadDataFromTemplate();
	void LoadConfigData();
	void Update(float deltaTime) override;

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
	void SetSerialized(bool value) {};

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

	template<typename MovingPlatform, typename ...ConstructorValues>
	void AddMovingPlatform(ConstructorValues... arguments) {
		static_assert(std::is_base_of<PlatformSubComponent, MovingPlatform>::value, "MovingPlatform must derive from PlatformSubComponent");
		auto hasPlatform = std::find_if(m_Platforms.begin(), m_Platforms.end(), [](const std::unique_ptr<PlatformSubComponent>& platform) {
			return platform->GetPlatformType() == MovingPlatform::SubComponentType;
			}) != m_Platforms.end();
			if (!hasPlatform) {
				m_Platforms.push_back(std::make_unique<MovingPlatform>(this, std::forward<ConstructorValues>(arguments)...));
			}
	}

	bool HasPlatform() { return !m_Platforms.empty(); }

	const PlatformSubComponent& GetPlatform() const {
		return *m_Platforms.at(0);
	}

	int32_t GetComponentId() const { return componentId; }
	
	// Make 
	const std::u16string& GetPathName() const { return m_PathName; }
	void SetPathName(const std::u16string& pathName) { m_PathName = pathName; }

	bool GetPathingStopped() const { return m_PathingStopped; }
	void SetPathingStopped(bool value) { m_PathingStopped = value; }

	uint32_t GetStartingWaypointIndex() const { return m_StartingWaypointIndex; }
	void SetStartingWaypointIndex(uint32_t value) { m_StartingWaypointIndex = value; }

	bool GetStartsIsInReverse() const { return m_StartsIsInReverse; }
	void SetStartsIsInReverse(bool value) { m_StartsIsInReverse = value; }

	bool GetStartOnload() const { return m_StartOnload; }
	void SetStartOnload(bool value) { m_StartOnload = value; }

	bool GetDirtyPathInfo() const { return m_DirtyPathInfo; }

private:
	/**
	 * The name of the path this platform is currently on
	 */
	std::u16string m_PathName;

	/**
	 * Whether the platform has stopped pathing
	 */
	bool m_PathingStopped = false;

	uint32_t m_StartingWaypointIndex = 0;

	bool m_StartsIsInReverse = false;

	int32_t componentId = -1;

	/**
	 * The mover sub component that belongs to this platform
	 */
	std::vector<std::unique_ptr<PlatformSubComponent>> m_Platforms;

	/**
	 * Whether the platform shouldn't auto start
	 */
	bool m_NoAutoStart;

	bool m_DirtyPathInfo = false;
	bool m_StartOnload = false;
};

#endif // MOVINGPLATFORMCOMPONENT_H
