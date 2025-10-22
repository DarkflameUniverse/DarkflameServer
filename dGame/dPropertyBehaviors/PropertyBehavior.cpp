#include "PropertyBehavior.h"

#include "Amf3.h"
#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"
#include "tinyxml2.h"
#include "ModelComponent.h"
#include "StringifiedEnum.h"

#include <ranges>

PropertyBehavior::PropertyBehavior(bool _isTemplated) {
	m_LastEditedState = BehaviorState::HOME_STATE;
	m_ActiveState = BehaviorState::HOME_STATE;

	// Starts off as true so that only specific ways of adding behaviors allow a new id to be requested.
	isTemplated = _isTemplated;
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
void PropertyBehavior::HandleMsg(GameMessages::RequestUse& msg) {
	m_States[m_ActiveState].HandleMsg(msg);
}

template<>
void PropertyBehavior::HandleMsg(GameMessages::ResetModelToDefaults& msg) {
	m_ActiveState = BehaviorState::HOME_STATE;
	for (auto& state : m_States | std::views::values) state.HandleMsg(msg);
}

void PropertyBehavior::CheckModifyState(BehaviorMessageBase& msg) {
	if (!isTemplated && m_BehaviorId != BehaviorMessageBase::DefaultBehaviorId) return;
	isTemplated = false;
	msg.SetNeedsNewBehaviorID(true);
}

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

void PropertyBehavior::Serialize(tinyxml2::XMLElement& behavior) const {
	behavior.SetAttribute("id", m_BehaviorId);
	behavior.SetAttribute("name", m_Name.c_str());
	behavior.SetAttribute("isLocked", isLocked);
	behavior.SetAttribute("isLoot", isLoot);

	// CUSTOM XML ATTRIBUTE
	behavior.SetAttribute("isTemplated", isTemplated);

	for (const auto& [stateId, state] : m_States) {
		if (state.IsEmpty()) continue;
		auto* const stateElement = behavior.InsertNewChildElement("State");
		stateElement->SetAttribute("id", static_cast<uint32_t>(stateId));
		state.Serialize(*stateElement);
	}
}


void PropertyBehavior::Deserialize(const tinyxml2::XMLElement& behavior) {
	m_Name = behavior.Attribute("name");
	behavior.QueryBoolAttribute("isLocked", &isLocked);
	behavior.QueryBoolAttribute("isLoot", &isLoot);

	// CUSTOM XML ATTRIBUTE
	if (!isTemplated) behavior.QueryBoolAttribute("isTemplated", &isTemplated);

	for (const auto* stateElement = behavior.FirstChildElement("State"); stateElement; stateElement = stateElement->NextSiblingElement("State")) {
		int32_t stateId = -1;
		stateElement->QueryIntAttribute("id", &stateId);
		if (stateId < 0 || stateId > 5) continue;
		m_States[static_cast<BehaviorState>(stateId)].Deserialize(*stateElement);
	}
}

void PropertyBehavior::Update(float deltaTime, ModelComponent& modelComponent) {
	auto& activeState = GetActiveState();
	UpdateResult updateResult{};
	activeState.Update(deltaTime, modelComponent, updateResult);
	if (updateResult.newState.has_value() && updateResult.newState.value() != m_ActiveState) {
		LOG("Behavior %llu is changing from state %s to %s", StringifiedEnum::ToString(m_ActiveState).data(), StringifiedEnum::ToString(updateResult.newState.value()).data());
		GameMessages::ResetModelToDefaults resetMsg{};
		resetMsg.bResetPos = false;
		resetMsg.bResetRot = false;
		resetMsg.bUnSmash = false;
		resetMsg.bResetBehaviors = false;
		modelComponent.OnResetModelToDefaults(resetMsg);
		HandleMsg(resetMsg);
		m_ActiveState = updateResult.newState.value();
	}
}

void PropertyBehavior::OnChatMessageReceived(const std::string& sMessage) {
	auto& activeState = GetActiveState();
	activeState.OnChatMessageReceived(sMessage);
}

void PropertyBehavior::OnHit() {
	auto& activeState = GetActiveState();
	activeState.OnHit();
}

State& PropertyBehavior::GetActiveState() {
	DluAssert(m_States.contains(m_ActiveState));
	return m_States[m_ActiveState];
}
