#include "ModelComponent.h"
#include "Entity.h"

#include "Game.h"
#include "Logger.h"

#include "BehaviorStates.h"
#include "ControlBehaviorMsgs.h"

ModelComponent::ModelComponent(const LWOOBJID& parentEntityId) : Component{ parentEntityId } {
	auto* const parentEntity = Game::entityManager->GetEntity(m_Parent);

	m_OriginalPosition = parentEntity->GetDefaultPosition();
	m_OriginalRotation = parentEntity->GetDefaultRotation();

	m_userModelID = parentEntity->GetVarAs<LWOOBJID>(u"userModelID");
}

void ModelComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate) {
	// ItemComponent Serialization.  Pets do not get this serialization.
	if (!Game::entityManager->GetEntity(m_Parent)->HasComponent(eReplicaComponentType::PET)) {
		outBitStream->Write1();
		outBitStream->Write<LWOOBJID>(m_userModelID != LWOOBJID_EMPTY ? m_userModelID : m_Parent);
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
	args.Insert("objectID", std::to_string(m_Parent));

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
	args.Insert("objectID", std::to_string(m_Parent));
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
