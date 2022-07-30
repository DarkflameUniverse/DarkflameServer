#ifndef REBUILDCOMPONENT_H
#define REBUILDCOMPONENT_H

#include <BitStream.h>
#include <vector>
#include <string>
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "ScriptedActivityComponent.h"
#include "Preconditions.h"
#include "Component.h"

class Entity;

/**
 * Component that handles entities that can be built into other entities using the quick build mechanic. Generally
 * consists of an activator that shows a popup and then the actual entity that the bricks are built into. Note
 * that quick builds are also scripted activities so this shared some logic with the ScriptedActivityComponent.
 */
class RebuildComponent : public Component {
public:
	static const uint32_t ComponentType = COMPONENT_TYPE_REBUILD;

	RebuildComponent(Entity* entity);
	~RebuildComponent() override;

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);
	void Update(float deltaTime) override;

	/**
	 * Handles a OnUse event from some entity, initiating the quick build
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Spawns the activator that can be used to initiate the rebuild
	 */
	void SpawnActivator();

	/**
	 * Despawns the activiator that can be used to initiate the rebuild
	 */
	void DespawnActivator();

	/**
	 * Returns the entity that acts as the activator for this rebuild
	 * @return the entity that acts as the activator for this rebuild
	 */
	Entity* GetActivator();

	/**
	 * Returns the spawn position of the activator for this rebuild, if any
	 * @return the spawn position of the activator for this rebuild, if any
	 */
	NiPoint3 GetActivatorPosition();

	/**
	 * Sets the spawn position for the activator of this rebuild
	 * @param value the spawn position to set for the activator
	 */
	void SetActivatorPosition(NiPoint3 value);

	/**
	 * Returns the time it takes for the rebuild to reset after being built
	 * @return the time it takes for the rebuild to reset after being built
	 */
	float GetResetTime();

	/**
	 * Sets the time it takes for the rebuild to reset after being built
	 * @param value the reset time to set
	 */
	void SetResetTime(float value);

	/**
	 * Returns the time it takes to complete the rebuild
	 * @return the time it takes to complete the rebuild
	 */
	float GetCompleteTime();

	/**
	 * Sets the time it takes to complete the rebuild
	 * @param value the completion time to set
	 */
	void SetCompleteTime(float value);

	/**
	 * Returns the imagination that's taken when completing the rebuild
	 * @return the imagination that's taken when completing the rebuild
	 */
	int GetTakeImagination();

	/**
	 * Sets the imagination that's taken when completing the rebuild
	 * @param value the imagination deduction to set
	 */
	void SetTakeImagination(int value);

	/**
	 * Returns if the rebuild can be interrupted, currently unused
	 * @return if the rebuild can be interrupted
	 */
	bool GetInterruptible();

	/**
	 * Sets whether or not the rebuild can be interrupted, currently unused
	 * @param value true if the rebuild may be interrupted, false otherwise
	 */
	void SetInterruptible(bool value);

	/**
	 * Returns whether or not this entity contains a built-in activator
	 * @return whether or not this entity contains a built-in activator
	 */
	bool GetSelfActivator();

	/**
	 * Sets whether or not this entity contains a built-in activator. If set to false this will spawn activators on
	 * each new rebuild.
	 * @param value whether or not this entity contains a built-in activator
	 */
	void SetSelfActivator(bool value);

	/**
	 * Currently unused
	 */
	std::vector<int> GetCustomModules();

	/**
	 * Currently unused
	 */
	void SetCustomModules(std::vector<int> value);

	/**
	 * Returns the activity ID for participating in this rebuild
	 * @return the activity ID for participating in this rebuild
	 */
	int GetActivityId();

	/**
	 * Sets the activity ID for participating in this rebuild
	 * @param value the activity ID to set
	 */
	void SetActivityId(int value);

	/**
	 * Currently unused
	 */
	int GetPostImaginationCost();

	/**
	 * Currently unused
	 */
	void SetPostImaginationCost(int value);

	/**
	 * Returns the time it takes for an incomplete rebuild to be smashed automatically
	 * @return the time it takes for an incomplete rebuild to be smashed automatically
	 */
	float GetTimeBeforeSmash();

	/**
	 * Sets the time it takes for an incomplete rebuild to be smashed automatically
	 * @param value the time to set
	 */
	void SetTimeBeforeSmash(float value);

	/**
	 * Returns the current rebuild state
	 * @return the current rebuild state
	 */
	eRebuildState GetState();

	/**
	 * Returns the player that is currently building this rebuild
	 * @return the player that is currently building this rebuild
	 */
	Entity* GetBuilder() const;

	/**
	 * Returns whether or not the player is repositioned when initiating the rebuild
	 * @return whether or not the player is repositioned when initiating the rebuild
	 */
	bool GetRepositionPlayer() const;

	/**
	 * Sets whether or not the player is repositioned when initiating the rebuild
	 * @param value whether or not the player is repositioned when initiating the rebuild
	 */
	void SetRepositionPlayer(bool value);

	/**
	 * Adds a callback that is called when the rebuild is completed
	 * @param callback the callback to add
	 */
	void AddRebuildCompleteCallback(const std::function<void(Entity* user)>& callback);

	/**
	 * Adds a callback when the rebuild state is updated
	 * @param callback the callback to add
	 */
	void AddRebuildStateCallback(const std::function<void(eRebuildState state)>& callback);

	/**
	 * Resets the rebuild
	 * @param failed whether or not the player failed to complete the rebuild, triggers an extra animation
	 */
	void ResetRebuild(bool failed);

	/**
	 * Cancels the rebuild if it wasn't completed
	 * @param builder the player that's currently building
	 * @param failReason the reason the rebuild was cancelled
	 * @param skipChecks whether or not to skip the check for the rebuild not being completed
	 */
	void CancelRebuild(Entity* builder, eFailReason failReason, bool skipChecks = false);
private:
	/**
	 * Whether or not the quickbuild state has been changed since we last serialized it.
	 */
	bool m_StateDirty = true;

	/**
	 * The state the rebuild is currently in
	 */
	eRebuildState m_State = eRebuildState::REBUILD_OPEN;

	/**
	 * The time that has passed since initiating the rebuild
	 */
	float m_Timer = 0;

	/**
	 * The time that has passed before completing the rebuild
	 */
	float m_TimerIncomplete = 0;

	/**
	 * The position that the rebuild activator is spawned at
	 */
	NiPoint3 m_ActivatorPosition = NiPoint3::ZERO;

	/**
	 * The entity that represents the rebuild activator
	 */
	Entity* m_Activator = nullptr;

	/**
	 * The ID of the entity that represents the rebuild activator
	 */
	LWOOBJID m_ActivatorId = LWOOBJID_EMPTY;

	/**
	 * Triggers the blinking that indicates that the rebuild is resetting
	 */
	bool m_ShowResetEffect = false;

	/**
	 * Currently unused
	 */
	float m_Taken = 0;

	/**
	 * The callbacks that are called when the rebuild is completed
	 */
	std::vector<std::function<void(Entity* user)>> m_RebuildCompleteCallbacks{};

	/**
	 * The callbacks that are called when the rebuild state is updated
	 */
	std::vector<std::function<void(eRebuildState state)>> m_RebuildStateCallbacks{};

	/**
	 * The time it takes for the rebuild to reset after being completed
	 */
	float m_ResetTime = 0;

	/**
	 * The time it takes to complete the rebuild
	 */
	float m_CompleteTime = 0;

	/**
	 * The imagination that's deducted when compeleting the rebuild
	 */
	int m_TakeImagination = 0;

	/**
	 * Currently unused
	 */
	bool m_Interruptible = false;

	/**
	 * Whether or not this rebuild entity also has an activator attached. If not a new one will be spawned
	 */
	bool m_SelfActivator = false;

	/**
	 * Currently unused
	 */
	std::vector<int> m_CustomModules{};

	/**
	 * The activity ID that players partake in when doing this rebuild
	 */
	int m_ActivityId = 0;

	/**
	 * Currently unused
	 */
	int m_PostImaginationCost = 0;

	/**
	 * The time it takes for the rebuild to reset when it's not completed yet
	 */
	float m_TimeBeforeSmash = 0;

	/**
	 * The time it takes to drain imagination
	 */
	float m_TimeBeforeDrain = 0;

	/**
	 * The amount of imagination that was drained when building this rebuild
	 */
	int m_DrainedImagination = 0;

	/**
	 * Whether to reposition the player or not when building
	 */
	bool m_RepositionPlayer = true;

	/**
	 * Currently unused
	 */
	float m_SoftTimer = 0;

	/**
	 * The ID of the entity that's currently building the rebuild
	 */
	LWOOBJID m_Builder = LWOOBJID_EMPTY;

	/**
	 * Preconditions to be met before being able to start the rebuild
	 */
	PreconditionExpression* m_Precondition = nullptr;

	/**
	 * Starts the rebuild for a certain entity
	 * @param user the entity to start the rebuild
	 */
	void StartRebuild(Entity* user);

	/**
	 * Completes the rebuild for an entity, dropping loot and despawning the activator
	 * @param user the entity that completed the rebuild
	 */
	void CompleteRebuild(Entity* user);
};

#endif // REBUILDCOMPONENT_H
