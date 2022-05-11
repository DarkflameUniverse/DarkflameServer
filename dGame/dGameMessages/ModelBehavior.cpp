#include "ModelBehavior.h"
#include "Game.h"
#include <algorithm>
#include "dLogger.h"

ModelBehavior::ModelBehavior(uint32_t behaviorID, bool isLoot, std::string behaviorName) {
    this->behaviorID = behaviorID;
    this->isLoot = isLoot;
    this->isLocked = false;
    this->behaviorName = behaviorName;
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
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble,
        std::string callbackID, double xPosition, double yPosition) 
{
	BehaviorAction* newAction = new BehaviorAction();
	newAction->actionName = actionName;
	newAction->parameterValueString = parameterValueString;
	newAction->parameterValueDouble = parameterValueDouble;
	newAction->callbackID = callbackID;
	newAction->xPosition = xPosition;
	newAction->yPosition = yPosition;
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

	this->isLoot = false;
}

void ModelBehavior::AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
		std::string callbackID, uint32_t actionIndex)
{
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	auto stripPositionIterator = strip->second.begin() + actionIndex;

	BehaviorAction* newAction = new BehaviorAction();
	newAction->actionName = actionName;
	newAction->parameterValueString = parameterValueString;
	newAction->parameterValueDouble = parameterValueDouble;
	newAction->callbackID = callbackID;
	newAction->parameterName = parameterName;

	// The x and y position is stored in all nodes.  
	newAction->xPosition = strip->second[0]->xPosition;
	newAction->yPosition = strip->second[0]->yPosition;

	strip->second.insert(stripPositionIterator, newAction);

	this->isLoot = false;
}

void ModelBehavior::RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex) {
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	auto originalPosition = strip->second.begin() + actionIndex;
	for (auto positionToErase = originalPosition; positionToErase != strip->second.end(); positionToErase++) {
		Game::logger->Log("ModelComponent", "Deleting element!\n");
		delete *positionToErase;
		*positionToErase = nullptr;
	}
	strip->second.erase(originalPosition, strip->second.end());

	this->isLoot = false;
}

void ModelBehavior::RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID) {
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);

	for (auto element : strip->second) {
		Game::logger->Log("ModelBehavior", "Deleting element!\n");
		delete element;
		element = nullptr;
	}

	strip->second.clear();

	state->second.erase(stripID);

	this->isLoot = false;
}

void ModelBehavior::Rename(std::string newName) {
	this->behaviorName = newName;

	this->isLoot = false;
}

void ModelBehavior::UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition) {
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	for (auto action : strip->second) {
		action->xPosition = xPosition;
		action->yPosition = yPosition;
	}

	this->isLoot = false;
}

void ModelBehavior::RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex) {
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);

	std::rotate(strip->second.begin() + dstActionIndex, strip->second.begin() + srcActionIndex, strip->second.end());

	this->isLoot = false;
}

void ModelBehavior::MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID) {
	auto srcState = states.find(srcStateID);
	auto srcStrip = srcState->second.find(srcStripID);
	auto originalPosition = srcStrip->second.begin() + srcActionIndex;

	auto dstState = states.find(dstStateID);
	auto dstStrip = dstState->second.find(dstStripID);

	dstStrip->second.insert(dstStrip->second.begin() + dstActionIndex, srcStrip->second.begin() + srcActionIndex, srcStrip->second.end());

	srcStrip->second.erase(originalPosition, srcStrip->second.end());

	this->isLoot = false;
}

void ModelBehavior::SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, double yPosition, double xPosition) {
	auto srcState = states.find(srcStateID);
	auto srcStrip = srcState->second.find(srcStripID);
	
	auto dstState = states.find(dstStateID);

	std::vector<BehaviorAction*> newStrip;
	for (auto action = srcStrip->second.begin() + srcActionIndex; action != srcStrip->second.end(); action++) {
		newStrip.push_back(*action);
		(*action)->yPosition = yPosition;
		(*action)->xPosition = xPosition;
	}
	dstState->second.insert(std::make_pair(dstStripID, newStrip));

	srcStrip->second.erase(srcStrip->second.begin() + srcActionIndex, srcStrip->second.end());

	this->isLoot = false;
}

void ModelBehavior::MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t dstActionIndex) {
	auto srcState = states.find(srcStateID);
	auto srcStrip = srcState->second.find(srcStripID);

	auto dstState = states.find(dstStateID);
	auto dstStrip = dstState->second.find(dstStripID);

	dstStrip->second.insert(dstStrip->second.begin() + dstActionIndex, srcStrip->second.begin(), srcStrip->second.end());

	srcState->second.erase(srcStripID);

	this->isLoot = false;
}

void ModelBehavior::UpdateAction(
            BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, 
	    	std::string callbackID, uint32_t actionIndex) 
{
	auto state = states.find(stateID);
	auto strip = state->second.find(stripID);
	auto action = *(strip->second.begin() + actionIndex);

	action->parameterName = parameterName;
	action->parameterValueString = parameterValueString;
	action->parameterValueDouble = parameterValueDouble;

	this->isLoot = false;
}

void ModelBehavior::VerifyStates() {
	uint32_t countOfStatesWithStrips = 0;
	BEHAVIORSTATE candidateToSwap = -1;
	// Check whether or not we only have 1 state with strips
	for (auto state : states) {
		if (state.second.size() > 0) {
			countOfStatesWithStrips++;
			candidateToSwap = state.first;
		}
	}
	// If exactly 1 state has strips, make it the home state.  
	if (candidateToSwap != -1 && countOfStatesWithStrips == 1 && candidateToSwap != eStates::HOME_STATE) {
		auto state = states.find(candidateToSwap);
		states.find(eStates::HOME_STATE)->second = state->second;
		for (auto strip : state->second) {
			strip.second.clear();
		}
		state->second.clear();
	}
}