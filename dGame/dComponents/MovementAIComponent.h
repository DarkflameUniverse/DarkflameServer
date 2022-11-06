/*
 * Darkflame Universe
 * Copyright 2018
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
 * actually handles attackig and following enemy entities.
 */
class MovementAIComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_MOVEMENT_AI;

	MovementAIComponent(Entity* parentEntity, MovementAIInfo info);
	~MovementAIComponent() override;

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
	void SetSpeed(float value);

	/**
	 * Returns the max speed at which this entity may run
	 * @return the max speed at which this entity may run
	 */
	float GetSpeed() const;

	/**
	 * Sets how fast the entity will accelerate when not running at full speed
	 * @param value the acceleration to set
	 */
	void SetAcceleration(float value);

	/**
	 * Returns the current speed at which this entity accelerates when not running at full speed
	 * @return the current speed at which this entity accelerates when not running at full speed
	 */
	float GetAcceleration() const;

	/**
	 * Sets the halting distance (the distance at which we consider the target to be reached)
	 * @param value the halting distance to set
	 */
	void SetHaltDistance(float value);

	/**
	 * Returns the current halting distance (the distance at which we consider the target to be reached)
	 * @return the current halting distance
	 */
	float GetHaltDistance() const;

	/**
	 * Sets the speed the entity is currently running at
	 * @param value the speed value to set
	 */
	void SetCurrentSpeed(float value);

	/**
	 * Returns the speed the entity is currently running at
	 * @return the speed the entity is currently running at
	 */
	float GetCurrentSpeed() const;

	/**
	 * Locks the rotation of this entity in place, depending on the argument
	 * @param value if true, the entity will be rotationally locked
	 */
	void SetLockRotation(bool value);

	/**
	 * Returns whether this entity is currently rotationally locked
	 * @return true if the entity is rotationally locked, false otherwise
	 */
	bool GetLockRotation() const;

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
	NiPoint3 GetNextWaypoint() const;

	/**
	 * Returns the current position of this entity
	 * @return the current position of this entity
	 */
	NiPoint3 GetCurrentPosition() const;

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
	 * Returns the time it will take to reach the final waypoint according to the current speed
	 * @return the time it will take to reach the final waypoint according to the current speed
	 */
	float GetTimer() const;

	/**
	 * Returns if the entity is at its final waypoint
	 * @return if the entity is at its final waypoint
	 */
	bool AtFinalWaypoint() const;

	/**
	 * Renders the entity stationary
	 */
	void Stop();

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
	void SetPath(std::vector<NiPoint3> path);

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
	float m_Speed;

	/**
	 * The time it will take to reach the next waypoint using the current speed
	 */
	float m_Timer;

	/**
	 * The total time it will take to reach the waypoint form its starting point
	 */
	float m_TotalTime;

	/**
	 * The path this entity is currently traversing
	 */
	uint32_t m_PathIndex;

	/**
	 * If the entity has reached it last waypoint
	 */
	bool m_Done;

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
	 * The base speed this entity has
	 */
	float m_BaseSpeed;

	/**
	 * If the AI is currently turned of (e.g. when teleporting to some location)
	 */
	bool m_Interrupted;

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
	std::vector<NiPoint3> m_CurrentPath;

	/**
	 * Queue of positions to traverse
	 */
	std::stack<NiPoint3> m_Queue;

	/**
	 * Cache of all lots and their respective speeds
	 */
	static std::map<LOT, float> m_PhysicsSpeedCache;
};

#endif // MOVEMENTAICOMPONENT_H
