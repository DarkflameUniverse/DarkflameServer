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

	BehaviorAction* newAction = new BehaviorAction();
	newAction->stateID = stateID;
	newAction->stripID = stripID;
	newAction->actionName = actionName;
	newAction->parameterValue = actionParameter;
	newAction->parameterValueNumber = actionParameterValue;
	newAction->callbackID = callbackID;
	newAction->xPosition = xPosition;
	newAction->yPosition = yPosition;
	newAction->behaviorID = behaviorID;
	newAction->behaviorName = behaviorName;
	newAction->parameterName = parameterName;

	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	if (strip == state->second.end()) {
		std::vector<BehaviorAction*> ba;
		ba.push_back(newAction);
		state->second.insert(std::make_pair(stripID, ba));
	} else {
		strip->second.push_back(newAction);
	}

	Game::logger->Log("ModelComponent", "Added new action to strip %i in state %i!\n", stripID, stateID);
}

void ModelComponent::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, uint32_t actionIndex)
{
	Game::logger->Log("ModelComponent", "Adding new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	auto stripPositionIterator = strip->second.begin() + actionIndex;

	BehaviorAction* newAction = new BehaviorAction();
	newAction->stateID = stateID;
	newAction->stripID = stripID;
	newAction->actionName = actionName;
	newAction->parameterValue = actionParameter;
	newAction->parameterValueNumber = actionParameterValue;
	newAction->callbackID = callbackID;
	newAction->behaviorID = strip->second[0]->behaviorID;
	newAction->parameterName = parameterName;
	newAction->xPosition = strip->second[0]->xPosition;
	newAction->yPosition = strip->second[0]->yPosition;
	newAction->behaviorName = strip->second[0]->behaviorName;

	strip->second.insert(stripPositionIterator, newAction);

	Game::logger->Log("ModelComponent", "Added new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}