#include "ModelBehavior.h"
#include "Game.h"
#include "dLogger.h"

ModelBehavior::ModelBehavior(uint32_t behaviorID) {
    this->behaviorID = behaviorID;
    this->isLoot = true;
    this->isLocked = false;
    this->behaviorName = "EmosewaMC";
}

ModelBehavior::~ModelBehavior() {
    // TODO manage pointers
}

void ModelBehavior::AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue,
        std::string callbackID, double xPosition, double yPosition, uint32_t behaviorID, std::string behaviorName) 
{
	Game::logger->Log("ModelBehavior", "Adding new action to strip %i in state %i!\n", stripID, stateID);

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

	Game::logger->Log("ModelBehavior", "Added new action to strip %i in state %i!\n", stripID, stateID);
}

void ModelBehavior::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
		std::string callbackID, uint32_t actionIndex, uint32_t behaviorID)
{
	Game::logger->Log("ModelBehavior", "Adding new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

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
    newAction->behaviorID = behaviorID;

	strip->second.insert(stripPositionIterator, newAction);

	Game::logger->Log("ModelBehavior", "Added new action to existing strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelBehavior::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, uint32_t behaviorID) {
	Game::logger->Log("ModelBehavior", "Removing action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);

	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	auto originalPosition = strip->second.begin() + actionIndex;
	for (auto positionToErase = originalPosition; positionToErase != strip->second.end(); positionToErase++) {
		Game::logger->Log("ModelComponent", "Deleting element!\n");
		delete *positionToErase;
		*positionToErase = nullptr;
	}
	strip->second.erase(originalPosition, strip->second.end());

	Game::logger->Log("ModelBehavior", "Removed action(s) from strip %i at position %i in state %i!\n", stripID, actionIndex, stateID);
}

void ModelBehavior::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t behaviorID) {
	Game::logger->Log("ModelBehavior", "Removing strip %i in state %i!\n", stripID, stateID);

	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);

	for (auto element : strip->second) {
		Game::logger->Log("ModelBehavior", "Deleting element!\n");
		delete element;
		element = nullptr;
	}

	strip->second.clear();

	state->second.erase(stripID);

	Game::logger->Log("ModelBehavior", "Removed strip %i in state %i!\n", stripID, stateID);
}