#include "ModelBehavior.h"
#include "Game.h"
#include <algorithm>
#include "dLogger.h"

ModelBehavior::ModelBehavior(uint32_t behaviorID, bool isLoot) {
    this->behaviorID = behaviorID;
    this->isLoot = isLoot;
    this->isLocked = false;
    this->behaviorName = "New Behavior";
}

ModelBehavior::~ModelBehavior() {
	for (auto state : states) {
		for (auto strip : state.second) {
			for (auto action : strip.second) {
				Game::logger->Log("ModelBehavior", "Deleting behavior (%s)\n", action->actionName.c_str());
				delete action;
				action = nullptr;
			}
			strip.second.clear();
		}
		state.second.clear();
	}
	states.clear();
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

void ModelBehavior::Rename(uint32_t behaviorID, std::string newName) {
	this->behaviorName = newName;
	this->isLoot = false;
}

void ModelBehavior::UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition, uint32_t behaviorID) {
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	for (auto action : strip->second) {
		action->xPosition = xPosition;
		action->yPosition = yPosition;
	}
}

void ModelBehavior::RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex, uint32_t behaviorID) {
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);

	std::rotate(strip->second.begin() + dstActionIndex, strip->second.begin() + srcActionIndex, strip->second.end());
}

void ModelBehavior::MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID) {
	auto srcState = states.find(srcStateID);
	auto srcStrip = srcState->second.find(srcStripID);
	auto originalPosition = srcStrip->second.begin() + srcActionIndex;

	auto dstState = states.find(dstStateID);
	auto dstStrip = dstState->second.find(dstStripID);

	dstStrip->second.insert(dstStrip->second.begin() + dstActionIndex, srcStrip->second.begin() + srcActionIndex, srcStrip->second.end());

	srcStrip->second.erase(originalPosition, srcStrip->second.end());

	for (auto element : dstStrip->second) {
		element->stateID = dstStateID;
		element->stripID = dstStripID;
	}
}

void ModelBehavior::SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, double yPosition, double xPosition) {
	auto srcState = states.find(srcStateID);
	auto srcStrip = srcState->second.find(srcStripID);
	
	auto dstState = states.find(dstStateID);

	std::vector<BehaviorAction*> newStrip;
	for (auto action = srcStrip->second.begin() + srcActionIndex; action != srcStrip->second.end(); action++) {
		newStrip.push_back(*action);
		(*action)->yPosition = yPosition;
		(*action)->xPosition = xPosition;
		(*action)->stripID = dstStripID;
		(*action)->stateID = dstStateID;
	}
	dstState->second.insert(std::make_pair(dstStripID, newStrip));

	srcStrip->second.erase(srcStrip->second.begin() + srcActionIndex, srcStrip->second.end());
}

void ModelBehavior::MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, uint32_t dstActionIndex) {
	auto srcState = states.find(srcStateID);
	auto srcStrip = srcState->second.find(srcStripID);

	auto dstState = states.find(dstStateID);
	auto dstStrip = dstState->second.find(dstStripID);

	dstStrip->second.insert(dstStrip->second.begin() + dstActionIndex, srcStrip->second.begin(), srcStrip->second.end());

	srcState->second.erase(srcStripID);
}

void ModelBehavior::UpdateAction(
            BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
	    	std::string callbackID, uint32_t actionIndex, uint32_t behaviorID) 
{
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	auto action = *(strip->second.begin() + actionIndex);

	action->parameterName = parameterName;
	action->parameterValue = actionParameter;
	action->parameterValueNumber = actionParameterValue;
}