#pragma once

#include <map>

#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "eReplicaComponentType.h"
#include "Amf3.h"

#include "BehaviorStates.h"
#include "AddStripMessage.h"
#include "Action.h"

class Action;
class Entity;

struct Strip {
	void AddStrip(AddStripMessage& msg) {
		m_Actions = msg.GetActionsToAdd();
		for (auto& action : m_Actions) {
			LOG("%s %s %f %s", action.GetType().c_str(), action.GetValueParameterName().c_str(), (float)action.GetValueParameterDouble(), action.GetValueParameterString().c_str());
		}
		m_Position = msg.GetPosition();
	};
	std::vector<Action> m_Actions;
	StripUiPosition m_Position;
};

struct State {
	void AddStrip(AddStripMessage& msg) {
		if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
			m_Strips.resize(msg.GetActionContext().GetStripId() + 1);
		}
		m_Strips[msg.GetActionContext().GetStripId()].AddStrip(msg);
	};
	std::vector<Strip> m_Strips;
};

struct PropertyBehavior {
	void AddStrip(AddStripMessage& msg) {
		m_States[msg.GetActionContext().GetStateId()].AddStrip(msg);
	};
	std::map<BehaviorState, State> m_States;
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

	std::map<int32_t, PropertyBehavior> m_Behaviors;
private:

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
