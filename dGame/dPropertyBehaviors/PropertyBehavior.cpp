#include "PropertyBehavior.h"

#include "Amf3.h"
#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"

PropertyBehavior::PropertyBehavior() {
	m_LastEditedState = BehaviorState::HOME_STATE;
}

template<>
void PropertyBehavior::HandleMsg(AddStripMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(AddActionMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(RearrangeStripMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(UpdateActionMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(UpdateStripUiMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(RemoveStripMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(RemoveActionsMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(SplitStripMessage& msg) {
	m_States[msg.GetSourceActionContext().GetStateId()].HandleMsg(msg);
	m_States[msg.GetDestinationActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetDestinationActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(MigrateActionsMessage& msg) {
	m_States[msg.GetSourceActionContext().GetStateId()].HandleMsg(msg);
	m_States[msg.GetDestinationActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetDestinationActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(MergeStripsMessage& msg) {
	m_States[msg.GetSourceActionContext().GetStateId()].HandleMsg(msg);
	m_States[msg.GetDestinationActionContext().GetStateId()].HandleMsg(msg);
	m_LastEditedState = msg.GetDestinationActionContext().GetStateId();
};

template<>
void PropertyBehavior::HandleMsg(RenameMessage& msg) {
	m_Name = msg.GetName();
};

template<>
void PropertyBehavior::HandleMsg(AddMessage& msg) {
	// TODO Parse the corresponding behavior xml file.
	m_BehaviorId = msg.GetBehaviorId();
	isLoot = m_BehaviorId != 7965;
};

void PropertyBehavior::SendBehaviorListToClient(AMFArrayValue& args) const {
	args.Insert("id", std::to_string(m_BehaviorId));
	args.Insert("name", m_Name);
	args.Insert("isLocked", isLocked);
	args.Insert("isLoot", isLoot);
}

void PropertyBehavior::VerifyLastEditedState() {
	if (!m_States[m_LastEditedState].IsEmpty()) return;

	for (const auto& [stateId, state] : m_States) {
		if (state.IsEmpty()) continue;

		LOG_DEBUG("Updating last edited state to %i because %i is empty.", stateId, m_LastEditedState);
		m_LastEditedState = stateId;
		return;
	}

	LOG_DEBUG("No states found, sending default state");

	m_LastEditedState = BehaviorState::HOME_STATE;
}

void PropertyBehavior::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* const stateArray = args.InsertArray("states");

	for (const auto& [stateId, state] : m_States) {
		if (state.IsEmpty()) continue;

		LOG_DEBUG("Serializing state %i", stateId);
		auto* const stateArgs = stateArray->PushArray();
		stateArgs->Insert("id", static_cast<double>(stateId));
		state.SendBehaviorBlocksToClient(*stateArgs);
	}

	auto* const executionState = args.InsertArray("executionState");
	executionState->Insert("stateID", static_cast<double>(m_LastEditedState));
	executionState->InsertArray("strips");

	// TODO Serialize the execution state of the behavior
}
