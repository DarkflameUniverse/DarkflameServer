#pragma once

#include <map>

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "eReplicaComponentType.h"
#include "Amf3.h"

#include "BehaviorMessageBase.h"

#include "BehaviorStates.h"
#include "StripUiPosition.h"
#include "Action.h"

class Entity;
class AddStripMessage;
class AddActionMessage;

class Strip {
public:
	template<typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args);
	StripUiPosition GetPosition() { return m_Position; }
	bool IsEmpty() { return m_Actions.empty(); }
private:
	std::vector<Action> m_Actions;
	StripUiPosition m_Position;
};

class State {
public:
	template<typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorBlocksToClient(AMFArrayValue& args);
	bool IsEmpty();
private:
	std::vector<Strip> m_Strips;
};

class PropertyBehavior {
public:
	template<typename Msg>
	void HandleMsg(Msg& msg);

	void SendBehaviorListToClient(AMFArrayValue& args);
	void SendBehaviorBlocksToClient(AMFArrayValue& args);
private:

	// The states this behavior has.
	std::map<BehaviorState, State> m_States;

	// The name of this behavior.
	std::string m_Name = "New Behavior";

	// Whether this behavior is locked and cannot be edited.
	bool isLocked = false;

	// Whether this behavior is custom or pre-fab.
	bool isLoot = false;

	BehaviorState m_LastEditedState = BehaviorState::HOME_STATE;
};

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class ModelComponent : public Component {
public:
	inline static const eReplicaComponentType ComponentType = eReplicaComponentType::MODEL;

	ModelComponent(Entity* parent);

	void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) override;

	/**
	 * Returns the original position of the model
	 * @return the original position of the model
	 */
	const NiPoint3& GetPosition() { return m_OriginalPosition; }

	/**
	 * Sets the original position of the model
	 * @param pos the original position to set
	 */
	void SetPosition(const NiPoint3& pos) { m_OriginalPosition = pos; }

	/**
	 * Returns the original rotation of the model
	 * @return the original rotation of the model
	 */
	const NiQuaternion& GetRotation() { return m_OriginalRotation; }

	/**
	 * Sets the original rotation of the model
	 * @param rot the original rotation to set
	 */
	void SetRotation(const NiQuaternion& rot) { m_OriginalRotation = rot; }

	template<typename Msg>
	void HandleControlBehaviorsMsg(AMFArrayValue* args) {
		static_assert(std::is_base_of_v<BehaviorMessageBase, Msg>, "Msg must be a BehaviorMessageBase");
		Msg msg(args);
		m_Behaviors[msg.GetBehaviorId()].HandleMsg(msg);
	};

	// Updates the pending behavior ID to the new ID.
	void UpdatePendingBehaviorId(const int32_t newId);

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
	void SendBehaviorListToClient(AMFArrayValue& args);

	void SendBehaviorBlocksToClient(int32_t behaviorToSend, AMFArrayValue& args);

private:
	std::map<int32_t, PropertyBehavior> m_Behaviors;

	/**
	 * The original position of the model
	 */
	NiPoint3 m_OriginalPosition;

	/**
	 * The rotation original of the model
	 */
	NiQuaternion m_OriginalRotation;

	/**
	 * The ID of the user that made the model
	 */
	LWOOBJID m_userModelID;

	// std::map<int32_t, Behavior> m_Behaviors;
};
