#include "ModelComponent.h"
#include "Entity.h"
#include "dLogger.h"

ModelComponent::ModelComponent(uint32_t componentID, Entity* parent) : Component(parent)
{
	m_Position = m_Parent->GetDefaultPosition();
	m_Rotation = m_Parent->GetDefaultRotation();

	m_userModelID = m_Parent->GetVarAs<LWOOBJID>(u"userModelID");

	/*
	for (auto set : m_Parent->GetInfo().settings) {
		if (set && set->GetKey() == u"userModelID") {
			m_userModelID = std::stoull(set->GetValueAsString());
		}
	}
	*/
}

ModelComponent::~ModelComponent() {
}

void ModelComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	//item component:
	outBitStream->Write1();
	outBitStream->Write<LWOOBJID>(m_userModelID);
	outBitStream->Write<int>(0);
	outBitStream->Write0();
	
	//actual model component:
	outBitStream->Write1(); //yes we are writing model info
	outBitStream->Write0(); //??
	outBitStream->Write<int>(2); //model type, always 2 for BBB

	outBitStream->Write(m_Position);
	outBitStream->Write(m_Rotation);

	outBitStream->Write1(); //second data flag, all unknown. Maybe skip?
	outBitStream->Write<int>(0);
	outBitStream->Write1();
	outBitStream->Write0();
}

void ModelComponent::AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, double xPosition, double yPosition, uint32_t behaviorID, std::string behaviorName)
{
	Game::logger->Log("ModelComponent", "Adding new action to strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->AddStrip(stateID, stripID, actionName, parameterName, actionParameter, actionParameterValue, callbackID, xPosition, yPosition, behaviorID, behaviorName);

	Game::logger->Log("ModelComponent", "Added new action to strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, uint32_t actionIndex, uint32_t behaviorID)
{
	Game::logger->Log("ModelComponent", "Adding new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->AddAction(stateID, stripID, actionName, parameterName, actionParameter, actionParameterValue, callbackID, actionIndex, behaviorID);

	Game::logger->Log("ModelComponent", "Added new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->RemoveAction(stateID, stripID, actionIndex, behaviorID);

	Game::logger->Log("ModelComponent", "Removed action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelComponent::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t behaviorID) {
	Game::logger->Log("ModelComponent", "Removing strip %i in state %i!\n", stripID, stateID);

	auto behavior = FindBehavior(behaviorID);

	behavior->RemoveStrip(stateID, stripID, behaviorID);

	Game::logger->Log("ModelComponent", "Removed strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::AddBehavior(uint32_t behaviorID, uint32_t behaviorIndex) {
	Game::logger->Log("ModelComponent", "Adding behavior %i in index %i!\n", behaviorID, behaviorIndex);
	if (behaviors.size() >= 5) return;
	for (auto behavior : behaviors) {
		// Don't allow duplicates.  For some reason the client won't render duplicates in the behaviors list.
		if (behavior->GetBehaviorID() == behaviorID) return;
	}
	auto behavior = new ModelBehavior(behaviorID);
	behaviors.insert(behaviors.begin() + behaviorIndex, behavior);
	Game::logger->Log("ModelComponent", "Added behavior %i in index %i!\n", behaviorID, behaviorIndex);
}

ModelBehavior* ModelComponent::FindBehavior(uint32_t behaviorID) {
	for (auto behavior : behaviors) {
		if (behavior->GetBehaviorID() == behaviorID) return behavior;
	}
	if (behaviors.size() < 5) {
		auto behavior = new ModelBehavior(behaviorID);
		behaviors.insert(behaviors.begin(), behavior);
		return behavior;
	}
	return nullptr;
}