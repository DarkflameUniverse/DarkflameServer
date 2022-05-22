#include "ModelBehavior.h"
#include "tinyxml2.h"

ModelBehavior::ModelBehavior(uint32_t behaviorID, ModelComponent* model, bool isLoot, std::string behaviorName) {
    this->behaviorID = behaviorID;
    this->isLoot = isLoot;
    this->isLocked = false;
    this->behaviorName = behaviorName;
	this->m_ModelComponent = model;
}

ModelBehavior::~ModelBehavior() {
	Game::logger->Log("ModelBehavior", "Deleting behavior %i\n", this->behaviorID);
	for (auto state : states) {
		delete state.second;
		state.second = nullptr;
	}
	states.clear();
}

void ModelBehavior::AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble,
        std::string callbackID, double xPosition, double yPosition) 
{
	BehaviorAction* newAction = new BehaviorAction();
	newAction->actionName = actionName;
	newAction->parameterValueString = parameterValueString;
	newAction->parameterValueDouble = parameterValueDouble;
	newAction->callbackID = callbackID;
	newAction->parameterName = parameterName;

	auto state = states.find(stateID);
	if (state == states.end()) {
		auto newState = new BehaviorState(stateID);
		newState->AddStrip(newAction, stripID, xPosition, yPosition, this);
		states.insert(std::make_pair(stateID, newState));
	} else {
		state->second->AddStrip(newAction, stripID, xPosition, yPosition, this);
	}

	this->isLoot = false;
}

void ModelBehavior::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
		std::string callbackID, uint32_t actionIndex)
{
	auto state = states.find(stateID);

	BehaviorAction* newAction = new BehaviorAction();

	newAction->actionName = actionName;
	newAction->parameterValueString = parameterValueString;
	newAction->parameterValueDouble = parameterValueDouble;
	newAction->callbackID = callbackID;
	newAction->parameterName = parameterName;

	state->second->AddAction(newAction, stripID, actionIndex);

	this->isLoot = false;
}

void ModelBehavior::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex) {
	auto state = states.find(stateID);

	state->second->RemoveAction(stripID, actionIndex);

	this->isLoot = false;
}

void ModelBehavior::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID) {
	auto state = states.find(stateID);

	state->second->RemoveStrip(stripID);

	if (state->second->GetStrips().size() == 0) {
		delete state->second;
		state->second = nullptr;
		states.erase(state->first);
		Game::logger->Log("ModelBehavior", "Erased state %i stateid is %i\n", state->first, stateID);
	}

	this->isLoot = false;
}

void ModelBehavior::Rename(std::string newName) {
	this->behaviorName = newName;

	this->isLoot = false;
}

void ModelBehavior::UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition) {
	auto state = states.find(stateID);

	state->second->UpdateUIOfStrip(stripID, xPosition, yPosition);

	this->isLoot = false;
}

void ModelBehavior::RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex) {
	auto state = states.find(stateID);
	
	state->second->RearrangeStrip(stripID, srcActionIndex, dstActionIndex);

	this->isLoot = false;
}

void ModelBehavior::MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID) {
	auto srcState = states.find(srcStateID)->second;

	auto dstState = states.find(dstStateID)->second;

	dstState->MigrateActions(srcState, srcActionIndex, srcStripID, dstActionIndex, dstStripID);

	this->isLoot = false;
}

void ModelBehavior::SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, double yPosition, double xPosition) {
	auto srcState = states.find(srcStateID)->second;
	
	auto dstState = states.find(dstStateID)->second;

	dstState->SplitStrip(srcState, srcActionIndex, srcStripID, dstStripID, yPosition, xPosition, this);

	this->isLoot = false;
}

void ModelBehavior::MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t dstActionIndex) {
	auto srcState = states.find(srcStateID)->second;

	auto dstState = states.find(dstStateID)->second;

	dstState->MergeStrips(srcState, srcStripID, dstStripID, dstActionIndex);

	this->isLoot = false;
}

void ModelBehavior::UpdateAction(
            BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
	    	std::string callbackID, uint32_t actionIndex) 
{
	auto state = states.find(stateID);

	state->second->UpdateAction(stripID, actionName, parameterName, parameterValueString, parameterValueDouble, callbackID, actionIndex);

	this->isLoot = false;
}

void ModelBehavior::VerifyStates() {
	uint32_t countOfStatesWithStrips = 0;
	BEHAVIORSTATE candidateToSwap = -1;
	// Check whether or not we only have 1 state with strips
	for (auto state : states) {
		if (state.second->CountOfStrips() > 0) {
			countOfStatesWithStrips++;
			candidateToSwap = state.first;
		}
	}
	// If exactly 1 state has strip(s), make it the home state.  
	if (candidateToSwap != -1 && countOfStatesWithStrips == 1 && candidateToSwap != eStates::HOME_STATE) {
		auto srcState = states.find(candidateToSwap);
		states.insert(std::make_pair(eStates::HOME_STATE, srcState->second));
		states.erase(candidateToSwap);
	}
}

void ModelBehavior::FindStarterBlocks() {
	if (states.find(m_ActiveState) == states.end()) return;
	states.find(m_ActiveState)->second->FindStarterBlocks(m_ModelComponent);
}

void ModelBehavior::OnInteract(Entity* originator) {
	if (states.find(m_ActiveState) == states.end()) return;
	states.find(m_ActiveState)->second->OnInteract(m_ModelComponent, originator);
}

void ModelBehavior::OnChatMessage(ModelComponent* modelComponent, Entity* originator, std::string& message) {
	if (states.find(m_ActiveState) == states.end()) return;
	states.find(m_ActiveState)->second->OnChatMessage(m_ModelComponent, originator, message);
}

void ModelBehavior::SetState(BEHAVIORSTATE stateID) {
	this->m_ActiveState = stateID;
	m_ModelComponent->CheckStarterBlocks();
}

void ModelBehavior::ResetStrips() {
	for (auto state : states) {
		state.second->ResetStrips();
	}
}

void ModelBehavior::OnAttack(ModelComponent* modelComponent, Entity* originator) {
	states.find(m_ActiveState)->second->OnAttack(m_ModelComponent, originator);
}

void ModelBehavior::OnStartup(ModelComponent* modelComponent) {
	states.find(m_ActiveState)->second->OnStartup(m_ModelComponent);
}

void ModelBehavior::OnTimer(ModelComponent* modelComponent) {
	states.find(m_ActiveState)->second->OnTimer(m_ModelComponent);
}

void ModelBehavior::LoadStatesFromXml(tinyxml2::XMLElement* doc) {
	auto stateInfo = doc->FirstChildElement("State");

	while (stateInfo != nullptr) {
		BEHAVIORSTATE stateID;

		stateInfo->QueryAttribute("stateID", &stateID);

		auto behaviorState = new BehaviorState(stateID);

		behaviorState->LoadStripsFromXml(stateInfo, this);

		states.insert(std::make_pair(stateID, behaviorState));
	
		stateInfo = stateInfo->NextSiblingElement();
	}
}
