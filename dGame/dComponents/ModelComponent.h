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

	ModelComponent(Entity* parent);

	void LoadBehaviors();

	void Serialize(RakNet::BitStream& outBitStream, bool bIsInitialUpdate) override;

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

	/**
	 * Main gateway for all behavior messages to be passed to their respective behaviors.
	 * 
	 * @tparam Msg The message type to pass
	 * @param args the arguments of the message to be deserialized
	 */
	template<typename Msg>
	void HandleControlBehaviorsMsg(const AMFArrayValue& args) {
		static_assert(std::is_base_of_v<BehaviorMessageBase, Msg>, "Msg must be a BehaviorMessageBase");
		Msg msg(args);
		for (auto& behavior : m_Behaviors) {
			if (behavior.GetBehaviorId() == msg.GetBehaviorId()) { 
				behavior.HandleMsg(msg);
				return;
			}
		}

		// If we somehow added more than 5 behaviors, resize to 5.
		if (m_Behaviors.size() > 5) m_Behaviors.resize(5);

		// Do not allow more than 5 to be added. The client UI will break if you do!
		if (m_Behaviors.size() == 5) return;

		auto newBehavior = m_Behaviors.insert(m_Behaviors.begin(), PropertyBehavior());
		// Generally if we are inserting a new behavior, it is because the client is creating a new behavior.
		// However if we are testing behaviors the behavior will not exist on the initial pass, so we set the ID here to that of the msg.
		// This will either set the ID to -1 (no change in the current default) or set the ID to the ID of the behavior we are testing.
		newBehavior->SetBehaviorId(msg.GetBehaviorId());
		newBehavior->HandleMsg(msg);
	};

	void AddBehavior(AddMessage& msg);

	void MoveToInventory(MoveToInventoryMessage& msg);

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
	void SendBehaviorListToClient(AMFArrayValue& args) const;

	void SendBehaviorBlocksToClient(int32_t behaviorToSend, AMFArrayValue& args) const;
	
	void VerifyBehaviors();

	std::array<std::pair<int32_t, std::string>, 5> GetBehaviorsForSave() const;

private:
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
	NiQuaternion m_OriginalRotation;

	/**
	 * The ID of the user that made the model
	 */
	LWOOBJID m_userModelID;
};
