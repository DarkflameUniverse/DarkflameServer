#ifndef QUICKBUILDCOMPONENT_H
#define QUICKBUILDCOMPONENT_H

#include "BitStream.h"
#include <vector>
#include <string>
#include "dCommonVars.h"
#include "NiPoint3.h"
#include "ScriptedActivityComponent.h"
#include "Preconditions.h"
#include "Component.h"
#include "eReplicaComponentType.h"
#include "eQuickBuildState.h"

class Entity;
enum class eQuickBuildFailReason : uint32_t;

/**
 * Component that handles entities that can be built into other entities using the quick build mechanic. Generally
 * consists of an activator that shows a popup and then the actual entity that the bricks are built into. Note
 * that quick builds are also scripted activities so this shared some logic with the ScriptedActivityComponent.
 */
class QuickBuildComponent : public Component {
public:
	constexpr static const eReplicaComponentType ComponentType = eReplicaComponentType::QUICK_BUILD;

	QuickBuildComponent(const LWOOBJID& parentEntityId);
	~QuickBuildComponent() override;

	/**
	 * TODO: Need to define move and move-assign constructors
	*/

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;
	void Update(float deltaTime) override;

	/**
	 * Handles a OnUse event from some entity, initiating the quick build
	 * @param originator the entity that triggered the event
	 */
	void OnUse(Entity* originator) override;

	/**
	 * Spawns the activator that can be used to initiate the quickbuild
	 */
	void SpawnActivator();

	/**
	 * Despawns the activiator that can be used to initiate the quickbuild
	 */
	void DespawnActivator();

	/**
	 * Returns the entity that acts as the activator for this quickbuild
	 * @return the entity that acts as the activator for this quickbuild
	 */
	Entity* GetActivator();

	/**
	 * Returns the spawn position of the activator for this quickbuild, if any
	 * @return the spawn position of the activator for this quickbuild, if any
	 */
	NiPoint3 GetActivatorPosition();

	/**
	 * Sets the spawn position for the activator of this quickbuild
	 * @param value the spawn position to set for the activator
	 */
	void SetActivatorPosition(NiPoint3 value);

	/**
	 * Returns the time it takes for the quickbuild to reset after being built
	 * @return the time it takes for the quickbuild to reset after being built
	 */
	float GetResetTime();

	/**
	 * Sets the time it takes for the quickbuild to reset after being built
	 * @param value the reset time to set
	 */
	void SetResetTime(float value);

	/**
	 * Returns the time it takes to complete the quickbuild
	 * @return the time it takes to complete the quickbuild
	 */
	float GetCompleteTime();

	/**
	 * Sets the time it takes to complete the quickbuild
	 * @param value the completion time to set
	 */
	void SetCompleteTime(float value);

	/**
	 * Returns the imagination that's taken when completing the quickbuild
	 * @return the imagination that's taken when completing the quickbuild
	 */
	int GetTakeImagination();

	/**
	 * Sets the imagination that's taken when completing the quickbuild
	 * @param value the imagination deduction to set
	 */
	void SetTakeImagination(int value);

	/**
	 * Returns if the quickbuild can be interrupted, currently unused
	 * @return if the quickbuild can be interrupted
	 */
	bool GetInterruptible();

	/**
	 * Sets whether or not the quickbuild can be interrupted, currently unused
	 * @param value true if the quickbuild may be interrupted, false otherwise
	 */
	void SetInterruptible(bool value);

	/**
	 * Returns whether or not this entity contains a built-in activator
	 * @return whether or not this entity contains a built-in activator
	 */
	bool GetSelfActivator();

	/**
	 * Sets whether or not this entity contains a built-in activator. If set to false this will spawn activators on
	 * each new quickbuild.
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
	 * Returns the activity ID for participating in this quickbuild
	 * @return the activity ID for participating in this quickbuild
	 */
	int GetActivityId();

	/**
	 * Sets the activity ID for participating in this quickbuild
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
	 * Returns the time it takes for an incomplete quickbuild to be smashed automatically
	 * @return the time it takes for an incomplete quickbuild to be smashed automatically
	 */
	float GetTimeBeforeSmash();

	/**
	 * Sets the time it takes for an incomplete quickbuild to be smashed automatically
	 * @param value the time to set
	 */
	void SetTimeBeforeSmash(float value);

	/**
	 * Returns the current quickbuild state
	 * @return the current quickbuild state
	 */
	eQuickBuildState GetState();

	/**
	 * Returns the player that is currently building this quickbuild
	 * @return the player that is currently building this quickbuild
	 */
	Entity* GetBuilder() const;

	/**
	 * Returns whether or not the player is repositioned when initiating the quickbuild
	 * @return whether or not the player is repositioned when initiating the quickbuild
	 */
	bool GetRepositionPlayer() const;

	/**
	 * Sets whether or not the player is repositioned when initiating the quickbuild
	 * @param value whether or not the player is repositioned when initiating the quickbuild
	 */
	void SetRepositionPlayer(bool value);

	/**
	 * Adds a callback that is called when the quickbuild is completed
	 * @param callback the callback to add
	 */
	void AddQuickBuildCompleteCallback(const std::function<void(Entity* user)>& callback);

	/**
	 * Adds a callback when the quickbuild state is updated
	 * @param callback the callback to add
	 */
	void AddQuickBuildStateCallback(const std::function<void(eQuickBuildState state)>& callback);

	/**
	 * Resets the quickbuild
	 * @param failed whether or not the player failed to complete the quickbuild, triggers an extra animation
	 */
	void ResetQuickBuild(bool failed);

	/**
	 * Cancels the quickbuild if it wasn't completed
	 * @param builder the player that's currently building
	 * @param failReason the reason the quickbuild was cancelled
	 * @param skipChecks whether or not to skip the check for the quickbuild not being completed
	 */
	void CancelQuickBuild(Entity* builder, eQuickBuildFailReason failReason, bool skipChecks = false);
private:
	/**
	 * Whether or not the quickbuild state has been changed since we last serialized it.
	 */
	bool m_StateDirty = true;

	/**
	 * The state the quickbuild is currently in
	 */
	eQuickBuildState m_State = eQuickBuildState::OPEN;

	/**
	 * The time that has passed since initiating the quickbuild
	 */
	float m_Timer = 0;

	/**
	 * The time that has passed before completing the quickbuild
	 */
	float m_TimerIncomplete = 0;

	/**
	 * The position that the quickbuild activator is spawned at
	 */
	NiPoint3 m_ActivatorPosition = NiPoint3Constant::ZERO;

	/**
	 * The entity that represents the quickbuild activator
	 */
	Entity* m_Activator = nullptr;

	/**
	 * The ID of the entity that represents the quickbuild activator
	 */
	LWOOBJID m_ActivatorId = LWOOBJID_EMPTY;

	/**
	 * Triggers the blinking that indicates that the quickbuild is resetting
	 */
	bool m_ShowResetEffect = false;

	/**
	 * Currently unused
	 */
	float m_Taken = 0;

	/**
	 * The callbacks that are called when the quickbuild is completed
	 */
	std::vector<std::function<void(Entity* user)>> m_QuickBuildCompleteCallbacks{};

	/**
	 * The callbacks that are called when the quickbuild state is updated
	 */
	std::vector<std::function<void(eQuickBuildState state)>> m_QuickBuildStateCallbacks{};

	/**
	 * The time it takes for the quickbuild to reset after being completed
	 */
	float m_ResetTime = 0;

	/**
	 * The time it takes to complete the quickbuild
	 */
	float m_CompleteTime = 0;

	/**
	 * The imagination that's deducted when completing the quickbuild
	 */
	int m_TakeImagination = 0;

	/**
	 * Currently unused
	 */
	bool m_Interruptible = false;

	/**
	 * Whether or not this quickbuild entity also has an activator attached. If not a new one will be spawned
	 */
	bool m_SelfActivator = false;

	/**
	 * Currently unused
	 */
	std::vector<int> m_CustomModules{};

	/**
	 * The activity ID that players partake in when doing this quickbuild
	 */
	int m_ActivityId = 0;

	/**
	 * Currently unused
	 */
	int m_PostImaginationCost = 0;

	/**
	 * The time it takes for the quickbuild to reset when it's not completed yet
	 */
	float m_TimeBeforeSmash = 0;

	/**
	 * The time it takes to drain imagination
	 */
	float m_TimeBeforeDrain = 0;

	/**
	 * The amount of imagination that was drained when building this quickbuild
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
	 * The ID of the entity that's currently building the quickbuild
	 */
	LWOOBJID m_Builder = LWOOBJID_EMPTY;

	/**
	 * Preconditions to be met before being able to start the quickbuild
	 */
	PreconditionExpression* m_Precondition = nullptr;

	/**
	 * Starts the quickbuild for a certain entity
	 * @param user the entity to start the quickbuild
	 */
	void StartQuickBuild(Entity* user);

	/**
	 * Completes the quickbuild for an entity, dropping loot and despawning the activator
	 * @param user the entity that completed the quickbuild
	 */
	void CompleteQuickBuild(Entity* user);
};

#endif // QUICKBUILDCOMPONENT_H
