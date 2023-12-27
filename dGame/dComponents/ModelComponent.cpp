#include "ModelComponent.h"
#include "Entity.h"

void Strip::AddStrip(AddStripMessage& msg) {
	m_Actions = msg.GetActionsToAdd();
	for (auto& action : m_Actions) {
		LOG("%s %s %f %s", action.GetType().c_str(), action.GetValueParameterName().c_str(), (float)action.GetValueParameterDouble(), action.GetValueParameterString().c_str());
	}
	m_Position = msg.GetPosition();
};

void Strip::SendBehaviorBlocksToClient(AMFArrayValue& args) {
	m_Position.SendBehaviorBlocksToClient(args);

	auto* actions = args.InsertArray("actions");
	for (auto& action : m_Actions) {
		action.SendBehaviorBlocksToClient(*actions);
	}
}

void State::AddStrip(AddStripMessage& msg) {
	if (m_Strips.size() <= msg.GetActionContext().GetStripId()) {
		m_Strips.resize(msg.GetActionContext().GetStripId() + 1);
	}
	m_Strips[msg.GetActionContext().GetStripId()].AddStrip(msg);
};

void State::SendBehaviorBlocksToClient(AMFArrayValue& args) {
	auto* strips = args.InsertArray("strips");
	for (int32_t stripId = 0; stripId < m_Strips.size(); stripId++) {
		auto& strip = m_Strips.at(stripId);

		auto* stripArgs = strips->PushArray();
		stripArgs->Insert("id", static_cast<double>(stripId));

		strip.SendBehaviorBlocksToClient(*stripArgs);
	}
};

void PropertyBehavior::AddStrip(AddStripMessage& msg) {
	m_States[msg.GetActionContext().GetStateId()].AddStrip(msg);
};

void PropertyBehavior::SendBehaviorListToClient(AMFArrayValue& args) {
	args.Insert("name", m_Name);
	args.Insert("isLocked", isLocked);
	args.Insert("isLoot", isLoot);
}

void PropertyBehavior::SendBehaviorBlocksToClient(AMFArrayValue& args) {
	auto* stateArray = args.InsertArray("states");
	for (auto&[stateId, state] : m_States) {
		auto* stateArgs = stateArray->PushArray();
		stateArgs->Insert("id", static_cast<double>(stateId));
		state.SendBehaviorBlocksToClient(*stateArgs);
	}
}

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

void ModelComponent::HandleControlBehaviorsMsg(AddStripMessage& msg) {
	m_Behaviors[msg.GetBehaviorId()].AddStrip(msg);
}

void ModelComponent::UpdatePendingBehaviorId(const int32_t newId) {
	m_Behaviors[newId] = m_Behaviors[BehaviorMessageBase::DefaultBehaviorId];
	m_Behaviors.erase(BehaviorMessageBase::DefaultBehaviorId);
}

void ModelComponent::SendBehaviorListToClient(AMFArrayValue& args) {
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));

	auto* behaviorArray = args.InsertArray("behaviors");
	for (auto& [behaviorId, behavior] : m_Behaviors) {
		auto* behaviorArgs = behaviorArray->PushArray();
		behaviorArgs->Insert("id", std::to_string(behaviorId));
		behavior.SendBehaviorListToClient(*behaviorArgs);
	}
}

void ModelComponent::SendBehaviorBlocksToClient(int32_t behaviorToSend, AMFArrayValue& args) {
	args.Insert("BehaviorID", std::to_string(behaviorToSend));
	args.Insert("objectID", std::to_string(m_Parent->GetObjectID()));
	m_Behaviors[behaviorToSend].SendBehaviorBlocksToClient(args);
}
