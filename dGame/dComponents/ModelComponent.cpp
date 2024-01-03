#include "ModelComponent.h"
#include "Entity.h"

#include "Game.h"
#include "Logger.h"

#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"

///////////////// Strip ///////////////////

template<>
void Strip::HandleMsg(AddStripMessage& msg) {
	m_Actions = msg.GetActionsToAdd();
	m_Position = msg.GetPosition();
};

template<>
void Strip::HandleMsg(AddActionMessage& msg) {
	if (msg.GetActionIndex() == -1) return;

	m_Actions.insert(m_Actions.begin() + msg.GetActionIndex(), msg.GetAction());
};

template<>
void Strip::HandleMsg(UpdateStripUiMessage& msg) {
	m_Position = msg.GetPosition();
};

template<>
void Strip::HandleMsg(RemoveStripMessage& msg) {
	m_Actions.clear();
};

template<>
void Strip::HandleMsg(RemoveActionsMessage& msg) {
	if (msg.GetActionIndex() >= m_Actions.size()) return;
	m_Actions.erase(m_Actions.begin() + msg.GetActionIndex(), m_Actions.end());
};

template<>
void Strip::HandleMsg(UpdateActionMessage& msg) {
	if (msg.GetActionIndex() >= m_Actions.size()) return;
	m_Actions.at(msg.GetActionIndex()) = msg.GetAction();
};

template<>
void Strip::HandleMsg(RearrangeStripMessage& msg) {
	if (msg.GetDstActionIndex() >= m_Actions.size() || msg.GetSrcActionIndex() >= m_Actions.size() || msg.GetSrcActionIndex() <= msg.GetDstActionIndex()) return;
	std::rotate(m_Actions.begin() + msg.GetDstActionIndex(), m_Actions.begin() + msg.GetSrcActionIndex(), m_Actions.end());
};

template<>
void Strip::HandleMsg(SplitStripMessage& msg) {
	if (msg.GetTransferredActions().empty() && !m_Actions.empty()) {
		auto startToMove = m_Actions.begin() + msg.GetSrcActionIndex();
		msg.SetTransferredActions(startToMove, m_Actions.end());
		m_Actions.erase(startToMove, m_Actions.end());
	} else {
		m_Actions = msg.GetTransferredActions();
		m_Position = msg.GetPosition();
	}
};

template<>
void Strip::HandleMsg(MergeStripsMessage& msg) {
	if (msg.GetMigratedActions().empty() && !m_Actions.empty()) {
		msg.SetMigratedActions(m_Actions.begin(), m_Actions.end());
		m_Actions.erase(m_Actions.begin(), m_Actions.end());
	} else {
		m_Actions.insert(m_Actions.begin() + msg.GetDstActionIndex(), msg.GetMigratedActions().begin(), msg.GetMigratedActions().end());
	}
};

template<>
void Strip::HandleMsg(MigrateActionsMessage& msg) {
	if (msg.GetMigratedActions().empty() && !m_Actions.empty()) {
		auto startToMove = m_Actions.begin() + msg.GetSrcActionIndex();
		msg.SetMigratedActions(startToMove, m_Actions.end());
		m_Actions.erase(startToMove, m_Actions.end());
	} else {
		m_Actions.insert(m_Actions.begin() + msg.GetDstActionIndex(), msg.GetMigratedActions().begin(), msg.GetMigratedActions().end());
	}
};

void Strip::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	m_Position.SendBehaviorBlocksToClient(args);

	auto* actions = args.InsertArray("actions");
	for (auto& action : m_Actions) {
		action.SendBehaviorBlocksToClient(*actions);
	}
};

///////////////// State ///////////////////

template<>
void State::HandleMsg(AddStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		m_Strips.resize(msg.GetActionContext().GetStripId() + 1);
	}
	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(AddActionMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(UpdateStripUiMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(RemoveActionsMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(RearrangeStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(UpdateActionMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(RemoveStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		return;
	}

	m_Strips.at(msg.GetActionContext().GetStripId()).HandleMsg(msg);
};

template<>
void State::HandleMsg(SplitStripMessage& msg) {
	if (msg.GetTransferredActions().empty()) {
		if (m_Strips.size() <= msg.GetSourceActionContext().GetStripId()) {
			return;
		}

		m_Strips.at(msg.GetSourceActionContext().GetStripId()).HandleMsg(msg);
	} else {
		if (m_Strips.size() <= msg.GetDestinationActionContext().GetStripId()) {
			m_Strips.resize(msg.GetDestinationActionContext().GetStripId() + 1);
		}

		m_Strips.at(msg.GetDestinationActionContext().GetStripId()).HandleMsg(msg);
	}
};

template<>
void State::HandleMsg(MergeStripsMessage& msg) {
	if (msg.GetMigratedActions().empty()) {
		if (m_Strips.size() <= msg.GetSourceActionContext().GetStripId()) {
			return;
		}

		m_Strips.at(msg.GetSourceActionContext().GetStripId()).HandleMsg(msg);
	} else {
		if (m_Strips.size() <= msg.GetDestinationActionContext().GetStripId()) {
			m_Strips.resize(msg.GetDestinationActionContext().GetStripId() + 1);
		}

		m_Strips.at(msg.GetDestinationActionContext().GetStripId()).HandleMsg(msg);
	}
};

template<>
void State::HandleMsg(MigrateActionsMessage& msg) {
	if (msg.GetMigratedActions().empty()) {
		if (m_Strips.size() <= msg.GetSourceActionContext().GetStripId()) {
			return;
		}

		m_Strips.at(msg.GetSourceActionContext().GetStripId()).HandleMsg(msg);
	} else {
		if (m_Strips.size() <= msg.GetDestinationActionContext().GetStripId()) {
			m_Strips.resize(msg.GetDestinationActionContext().GetStripId() + 1);
		}

		m_Strips.at(msg.GetDestinationActionContext().GetStripId()).HandleMsg(msg);
	}
};

bool State::IsEmpty() const {
	for (auto& strip : m_Strips) {
		if (!strip.IsEmpty()) return false;
	}
	return true;
}

void State::SendBehaviorBlocksToClient(AMFArrayValue& args) const {
	auto* strips = args.InsertArray("strips");
	for (int32_t stripId = 0; stripId < m_Strips.size(); stripId++) {
		auto& strip = m_Strips.at(stripId);
		if (strip.IsEmpty()) continue;

		auto* stripArgs = strips->PushArray();
		stripArgs->Insert("id", static_cast<double>(stripId));

		strip.SendBehaviorBlocksToClient(*stripArgs);
	}
};

///////////////// PropertyBehavior ///////////////////

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

void PropertyBehavior::SetBehaviorId(int32_t behaviorId) {
	m_BehaviorId = behaviorId;
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
	auto* stateArray = args.InsertArray("states");

	auto lastState = BehaviorState::HOME_STATE;
	for (auto& [stateId, state] : m_States) {
		if (state.IsEmpty()) continue;

		LOG_DEBUG("Serializing state %i", stateId);
		auto* stateArgs = stateArray->PushArray();
		stateArgs->Insert("id", static_cast<double>(stateId));
		state.SendBehaviorBlocksToClient(*stateArgs);
	}

	auto* executionState = args.InsertArray("executionState");
	executionState->Insert("stateID", static_cast<double>(m_LastEditedState));
	executionState->InsertArray("strips");
}

//////////////////////// ModelComponent ////////////////////////

ModelComponent::ModelComponent(Entity* parent) : Component(parent) {
	m_OriginalPosition = m_Parent->GetDefaultPosition();
	m_OriginalRotation = m_Parent->GetDefaultRotation();

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");
}

void ModelComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	// ItemComponent Serialization.  Pets do not get this serialization.
	if (!m_Parent->HasComponent(eReplicaComponentType::PET)) {
		outBitStream->Write1();
		outBitStream->Write<LWOOBJID>(m_userModelID != LWOOBJID_EMPTY ? m_userModelID : m_Parent->GetObjectID());
		outBitStream->Write<int>(0);
		outBitStream->Write0();
	}

	//actual model component:
	outBitStream->Write1(); // Yes we are writing model info
	outBitStream->Write0(); // Is pickable
	outBitStream->Write<uint32_t>(2); // Physics type
	outBitStream->Write(m_OriginalPosition); // Original position
	outBitStream->Write(m_OriginalRotation); // Original rotation

	outBitStream->Write1(); // We are writing behavior info
	outBitStream->Write<uint32_t>(0); // Number of behaviors
	outBitStream->Write1(); // Is this model paused
	if (bIsInitialUpdate) outBitStream->Write0(); // We are not writing model editing info
}

void ModelComponent::UpdatePendingBehaviorId(const int32_t newId) {
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == -1) behavior.SetBehaviorId(newId);
}

void ModelComponent::SendBehaviorListToClient(AMFArrayValue& args) const {
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));

	auto* behaviorArray = args.InsertArray("behaviors");
	for (auto& behavior : m_Behaviors) {
		auto* behaviorArgs = behaviorArray->PushArray();
		behavior.SendBehaviorListToClient(*behaviorArgs);
	}
}

void ModelComponent::VerifyBehaviors() {
	for (auto& behavior : m_Behaviors) behavior.VerifyLastEditedState();
}

void ModelComponent::SendBehaviorBlocksToClient(int32_t behaviorToSend, AMFArrayValue& args) const {
	args.Insert("BehaviorID", std::to_string(behaviorToSend));
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == behaviorToSend) behavior.SendBehaviorBlocksToClient(args);
}

void ModelComponent::AddBehavior(AddMessage& msg) {
	// Can only have 1 of the loot behaviors
	for (auto& behavior : m_Behaviors) if (behavior.GetBehaviorId() == msg.GetBehaviorId()) return;
	m_Behaviors.insert(m_Behaviors.begin() + msg.GetBehaviorIndex(), PropertyBehavior());
	m_Behaviors.at(msg.GetBehaviorIndex()).HandleMsg(msg);
}

void ModelComponent::MoveToInventory(MoveToInventoryMessage& msg) {
	if (msg.GetBehaviorIndex() >= m_Behaviors.size() || m_Behaviors.at(msg.GetBehaviorIndex()).GetBehaviorId() != msg.GetBehaviorId()) return;
	m_Behaviors.erase(m_Behaviors.begin() + msg.GetBehaviorIndex());
	// TODO move to the inventory
}
