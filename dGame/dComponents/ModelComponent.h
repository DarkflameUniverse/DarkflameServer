#pragma once

#include <array>
#include <map>

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "eReplicaComponentType.h"

#include "Action.h"
#include "PropertyBehavior.h"
#include "StripUiPosition.h"

class AddMessage;
class AMFArrayValue;
class BehaviorMessageBase;
class Entity;
class MoveToInventoryMessage;

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class ModelComponent final : public Component {
public:
	static constexpr eReplicaComponentType ComponentType = eReplicaComponentType::MODEL;

	ModelComponent(Entity* parent, const int32_t componentID);

	void LoadBehaviors();
	void Update(float deltaTime) override;

	bool OnRequestUse(GameMessages::RequestUse& requestUse);
	bool OnResetModelToDefaults(GameMessages::ResetModelToDefaults& resetModelToDefaults);
	bool OnGetObjectReportInfo(GameMessages::GetObjectReportInfo& reportInfo);

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Returns the original position of the model
	 * @return the original position of the model
	 */
	const NiPoint3& GetOriginalPosition() { return m_OriginalPosition; }

	/**
	 * Sets the original position of the model
	 * @param pos the original position to set
	 */
	void SetPosition(const NiPoint3& pos) { m_OriginalPosition = pos; }

	/**
	 * Returns the original rotation of the model
	 * @return the original rotation of the model
	 */
	const NiQuaternion& GetOriginalRotation() { return m_OriginalRotation; }

	/**
	 * Sets the original rotation of the model
	 * @param rot the original rotation to set
	 */
	void SetRotation(const NiQuaternion& rot) { m_OriginalRotation = rot; }

	/**
	 * Main gateway for all behavior messages to be passed to their respective behaviors.
	 *
	 * @tparam Msg The message type to pass
	 * @param args the arguments of the message to be deserialized
	 *
	 * @return returns true if a new behaviorID is needed.
	 */
	template<typename Msg>
	bool HandleControlBehaviorsMsg(const AMFArrayValue& args) {
		static_assert(std::is_base_of_v<BehaviorMessageBase, Msg>, "Msg must be a BehaviorMessageBase");
		Msg msg{ args };
		for (auto&& behavior : m_Behaviors) {
			if (behavior.GetBehaviorId() == msg.GetBehaviorId()) {
				behavior.CheckModifyState(msg);
				behavior.HandleMsg(msg);
				return msg.GetNeedsNewBehaviorID();
			}
		}

		// If we somehow added more than 5 behaviors, resize to 5.
		if (m_Behaviors.size() > 5) m_Behaviors.resize(5);

		// Do not allow more than 5 to be added. The client UI will break if you do!
		if (m_Behaviors.size() == 5) return false;

		auto newBehavior = m_Behaviors.insert(m_Behaviors.begin(), PropertyBehavior());
		// Generally if we are inserting a new behavior, it is because the client is creating a new behavior.
		// However if we are testing behaviors the behavior will not exist on the initial pass, so we set the ID here to that of the msg.
		// This will either set the ID to -1 (no change in the current default) or set the ID to the ID of the behavior we are testing.
		newBehavior->SetBehaviorId(msg.GetBehaviorId());
		newBehavior->CheckModifyState(msg);
		newBehavior->HandleMsg(msg);
		return msg.GetNeedsNewBehaviorID();
	};

	void ProgressAddBehaviorMission(Entity& playerEntity);

	void AddBehavior(AddMessage& msg);

	void RemoveBehavior(MoveToInventoryMessage& msg, const bool keepItem);

	// Updates the pending behavior ID to the new ID.
	void UpdatePendingBehaviorId(const LWOOBJID newId, const LWOOBJID oldId);

	// Sends the behavior list to the client.

	/**
	 * The behaviors AMFArray will have up to 5 elements in the dense portion.
	 * Each element in the dense portion will be made up of another AMFArray
	 * with the following information mapped in the associative portion
	 * "id": Behavior ID cast to an AMFString
	 * "isLocked": AMFTrue or AMFFalse of whether or not the behavior is locked
	 * "isLoot": AMFTrue or AMFFalse of whether or not the behavior is a custom behavior (true if custom)
	 * "name": The name of the behavior formatted as an AMFString
	 */
	void SendBehaviorListToClient(AMFArrayValue& args) const;

	void SendBehaviorBlocksToClient(const LWOOBJID behaviorToSend, AMFArrayValue& args) const;

	void VerifyBehaviors();

	std::array<std::pair<LWOOBJID, std::string>, 5> GetBehaviorsForSave() const;

	const std::vector<PropertyBehavior>& GetBehaviors() const { return m_Behaviors; };

	void AddInteract();
	void RemoveInteract();

	void Pause() { m_Dirty = true; m_IsPaused = true; }

	void AddUnSmash();
	void RemoveUnSmash();
	bool IsUnSmashing() const { return m_NumActiveUnSmash != 0; }

	void Resume();

	// Attempts to set the velocity of an axis for movement.
	// If the axis currently has a velocity of zero, returns true.
	// If the axis is currently controlled by a behavior, returns false.
	bool TrySetVelocity(const NiPoint3& velocity) const;

	// Force sets the velocity to a value.
	void SetVelocity(const NiPoint3& velocity) const;

	// Attempts to claim a world axis (0 = x, 1 = y, 2 = z) for rotation in direction (+1 or -1).
	// Returns false if the axis is already controlled by a behavior.
	bool TryStartRotation(const int axis, const float direction);

	// Sets how many signed degrees the active rotation on axis has progressed and updates the entity.
	void SetRotationProgress(const int axis, const float degrees);

	// Releases the axis so another behavior can rotate it.
	void StopRotation(const int axis);

	// Degrees per second for rotation actions at the current speed.
	float GetAngularSpeed() const noexcept { return BASE_ANGULAR_SPEED * m_Speed; }

	void OnChatMessageReceived(const std::string& sMessage);

	void OnHit();

	// Sets the speed of the model
	void SetSpeed(const float newSpeed);

	// Whether or not to restart at the end of the frame
	void RestartAtEndOfFrame() { m_RestartAtEndOfFrame = true; }

	// Increments the number of strips listening for an attack.
	// If this is the first strip adding an attack, it will set the factions to the correct values.
	void AddAttack();

	// Decrements the number of strips listening for an attack.
	// If this is the last strip removing an attack, it will reset the factions to the default of -1.
	void RemoveAttack();

	float GetSpeed() const noexcept { return m_Speed; }
private:
	// Degrees per second per unit of m_Speed
	static constexpr float BASE_ANGULAR_SPEED = 15.0f;

	// Sends the client-side angular velocity for the currently active rotation axes.
	void SyncAngularVelocity() const;

	// Clears all rotation state and makes rotation relative to newBase.
	void ResetRotationState(const NiQuaternion& newBase);

	// Loads a behavior from the database.
	void LoadBehavior(const LWOOBJID behaviorID, const size_t index, const bool isIndexed);

	// Writes a behavior to a string so it can be saved.
	std::string SaveBehavior(const PropertyBehavior& behavior) const;

	// Number of Actions that are awaiting an UnSmash to finish.
	uint32_t m_NumActiveUnSmash{};

	// Whether or not this component needs to have its extra data serialized.
	bool m_Dirty{};

	// The number of strips listening for a RequestUse GM to come in.
	uint32_t m_NumListeningInteract{};

	// The number of strips listening for an attack.
	uint32_t m_NumActiveAttack{};

	// Whether or not the model is paused and should reject all interactions regarding behaviors.
	bool m_IsPaused{};
	/**
	 * The behaviors of the model
	 * Note: This is a vector because the order of the behaviors matters when serializing to the client.
	 * Note: No two PropertyBehaviors should have the same behavior ID.
	 */
	std::vector<PropertyBehavior> m_Behaviors;

	/**
	 * The original position of the model
	 */
	NiPoint3 m_OriginalPosition;

	/**
	 * The rotation original of the model
	 */
	NiQuaternion m_OriginalRotation = QuatUtils::IDENTITY;

	/**
	 * The ID of the user that made the model
	 */
	LWOOBJID m_userModelID;

	// The speed at which this model moves
	float m_Speed{ 3.0f };

	// Whether or not to restart at the end of the frame.
	bool m_RestartAtEndOfFrame{ false };

	// The rotation that m_RotationDegrees is applied on top of
	NiQuaternion m_RotationBase = QuatUtils::IDENTITY;

	// Accumulated signed degrees per world axis since m_RotationBase was set
	NiPoint3 m_RotationDegrees{};

	// m_RotationDegrees at the moment the current rotation on each axis started
	NiPoint3 m_RotationActionStart{};

	// Per axis -1, 0 or 1. Non-zero means a behavior currently owns rotation on that axis.
	NiPoint3 m_RotationDirection{};

	// Whether the client is sent an angular velocity to extrapolate rotation between updates
	bool m_SendAngularVelocity{ true };
};
