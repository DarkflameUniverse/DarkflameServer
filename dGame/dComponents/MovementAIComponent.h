/*
 * Darkflame Universe
 * Copyright 2023
 */

#ifndef MOVEMENTAICOMPONENT_H
#define MOVEMENTAICOMPONENT_H

#include "BitStream.h"
#include "Entity.h"
#include "GameMessages.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "Component.h"
#include "eReplicaComponentType.h"
#include <vector>

class ControllablePhysicsComponent;
class BaseCombatAIComponent;

/**
 * Information that describes the different variables used to make an entity move around
 */
struct MovementAIInfo {
	std::string movementType;

	/**
	 * The radius that the entity can wander in
	 */
	float wanderRadius;

	/**
	 * The speed at which the entity wanders
	 */
	float wanderSpeed;

	/**
	 * This is only used for the emotes
	 */
	float wanderChance;

	/**
	 * The min amount of delay before wandering
	 */
	float wanderDelayMin;

	/**
	 * The max amount of delay before wandering
	 */
	float wanderDelayMax;
};

/**
 * Component that handles the movement settings of an entity. Not to be confused with the BaseCombatAI component that
 * actually handles attacking and following enemy entities.
 */
class MovementAIComponent : public Component {
public:
	static const eReplicaComponentType ComponentType = eReplicaComponentType::MOVEMENT_AI;

	MovementAIComponent(Entity* parentEntity, MovementAIInfo info);

	void Update(float deltaTime) override;

	/**
	 * Returns the basic settings that this entity uses to move around
	 * @return the basic settings that this entity uses to move around
	 */
	const MovementAIInfo& GetInfo() const;

	/**
	 * Set a destination point for the entity to move towards
	 * @param value the destination point to move towards
	 */
	void SetDestination(const NiPoint3& value);

	/**
	 * Returns the current rotation this entity is moving towards
	 * @return the current rotation this entity is moving towards
	 */
	NiPoint3 GetDestination() const;

	/**
	 * Sets the max speed at which this entity may run
	 * @param value the speed value to set
	 */
	void SetMaxSpeed(float value);

	/**
	 * Sets how fast the entity will accelerate when not running at full speed
	 * @param value the acceleration to set
	 */
	void SetAcceleration(float value) { m_Acceleration = value; };

	/**
	 * Returns the current speed at which this entity accelerates when not running at full speed
	 * @return the current speed at which this entity accelerates when not running at full speed
	 */
	float GetAcceleration() const { return m_Acceleration; };

	/**
	 * Sets the halting distance (the distance at which we consider the target to be reached)
	 * @param value the halting distance to set
	 */
	void SetHaltDistance(float value) { m_HaltDistance = value; }

	/**
	 * Returns the current halting distance (the distance at which we consider the target to be reached)
	 * @return the current halting distance
	 */
	float GetHaltDistance() const { return m_HaltDistance; }

	/**
	 * Sets the speed the entity is currently running at
	 * @param value the speed value to set
	 */
	void SetCurrentSpeed(float value) { m_CurrentSpeed = value; }

	/**
	 * Returns the speed the entity is currently running at
	 * @return the speed the entity is currently running at
	 */
	float GetCurrentSpeed() const { return m_CurrentSpeed; }

	/**
	 * Locks the rotation of this entity in place, depending on the argument
	 * @param value if true, the entity will be rotationally locked
	 */
	void SetLockRotation(bool value) { m_LockRotation = value; }

	/**
	 * Returns whether this entity is currently rotationally locked
	 * @return true if the entity is rotationally locked, false otherwise
	 */
	bool GetLockRotation() const { return m_LockRotation; };

	/**
	 * Attempts to update the waypoint index, making the entity move to the next waypoint
	 * @return true if the waypoint could be increased, false if the entity is at the last waypoint already
	 */
	bool AdvanceWaypointIndex();

	/**
	 * Returns the waypoint the entity is currently moving towards
	 * @return the waypoint the entity is currently moving towards
	 */
	NiPoint3 GetCurrentWaypoint() const;

	/**
	 * Returns the waypoint this entity is supposed to move towards next
	 * @return the waypoint this entity is supposed to move towards next
	 */
	NiPoint3 GetNextWaypoint() const { return m_NextWaypoint; }

	/**
	 * Returns the approximate current location of the entity, including y coordinates
	 * @return the approximate current location of the entity
	 */
	NiPoint3 ApproximateLocation() const;

	/**
	 * Teleports this entity to a position. If the distance between the provided point and the y it should have
	 * according to map data, this will not succeed (to avoid teleporting entities into the sky).
	 * @param point the point to teleport to
	 * @return true if the warp was successful, false otherwise
	 */
	bool Warp(const NiPoint3& point);

	/**
	 * Returns if the entity is at its final waypoint
	 * @return if the entity is at its final waypoint
	 */
	bool AtFinalWaypoint() const { return m_AtFinalWaypoint; }

	bool IsPaused() const { return m_IsPaused; }

	/**
	 * Pauses the current pathing of this entity. The current path waypoint will be saved for resuming later.
	 */
	void Pause();

	/**
	 * Resumes pathing from the current position to the destination that was set
	 * when the entity was paused.
	 */
	void Resume();

	/**
	 * Renders the entity stationary
	 */
	void Stop();

	void ReversePath();

	void HandleWaypointArrived();
	
	void SetupPath(const std::string& pathname);

	/**
	 * Stops the current movement and moves the entity to a certain point. Will continue until it's close enough,
	 * after which its AI is enabled again.
	 * @param point the point to move towards
	 */
	void PullToPoint(const NiPoint3& point);

	/**
	 * Sets a path to follow for the AI
	 * @param path the path to follow
	 */
	void SetPath(std::vector<NiPoint3> path, bool startsInReverse = false);

	// Advance the path waypoint index and return if there is a next waypoint
	bool AdvancePathWaypointIndex();

	const NiPoint3& GetCurrentPathWaypoint() const;

	/**
	 * Returns the base speed from the DB for a given LOT
	 * @param lot the lot to check for
	 * @return the base speed of the lot
	 */
	static float GetBaseSpeed(LOT lot);

private:

	/**
	 * Sets the current position of the entity
	 * @param value the position to set
	 */
	void SetPosition(const NiPoint3& value);

	/**
	 * Sets the current rotation of the entity
	 * @param value the rotation to set
	 */
	void SetRotation(const NiQuaternion& value);

	/**
	 * Sets the current velocity of the entityes
	 * @param value the velocity to set
	 */
	void SetVelocity(const NiPoint3& value);

	/**
	 * Base information regarding the movement information for this entity
	 */
	MovementAIInfo m_Info;

	/**
	 * The point this entity is moving towards
	 */
	NiPoint3 m_NextWaypoint;

	/**
	 * The max speed this entity may move at
	 */
	float m_MaxSpeed;

	/**
	 * The time it will take to reach the next waypoint using the current speed
	 */
	float m_TimeTravelled;

	/**
	 * The path this entity is currently traversing
	 */
	uint32_t m_PathIndex;

	/**
	 * If the entity has reached it last waypoint
	 */
	bool m_AtFinalWaypoint;

	/**
	 * The speed the entity is currently moving at
	 */
	float m_CurrentSpeed;

	/**
	 * The acceleration this entity has when not moving at its top speed yet
	 */
	float m_Acceleration;

	/**
	 * The distance between the current position and the target waypoint to consider it reached (to not ghost into it).
	 */
	float m_HaltDistance;

	/**
	 * The total time it will take to reach the waypoint form its starting point
	 */
	float m_TimeToTravel;

	/**
	 * The base speed this entity has
	 */
	float m_BaseSpeed;

	/**
	 * If the AI is currently turned of (e.g. when teleporting to some location)
	 */
	bool m_PullingToPoint;

	/**
	 * A position that the entity is currently moving towards while being interrupted
	 */
	NiPoint3 m_PullPoint;

	/**
	 * If the entity is currently rotationally locked
	 */
	bool m_LockRotation;

	/**
	 * Optional direct link to the combat AI component of the parent entity
	 */
	BaseCombatAIComponent* m_BaseCombatAI = nullptr;

	/**
	 * The path the entity is currently following
	 */
	std::vector<NiPoint3> m_InterpolatedWaypoints;

	/**
	 * The path from the current position to the destination.
	 */
	std::vector<NiPoint3> m_CurrentPath;

	/**
	 * The index of the current waypoint in the path
	 */
	int32_t m_CurrentPathWaypointIndex;

	/**
	 * The index of the current waypoint in the path
	 */
	int32_t m_NextPathWaypointIndex;

	/**
	 * Whether or not the path is being read in reverse 
	 */
	bool m_IsInReverse;

	/**
	 * Whether or not the current movement via pathing is paused.
	 */
	bool m_IsPaused;
};

#endif // MOVEMENTAICOMPONENT_H
