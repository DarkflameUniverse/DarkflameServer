#include "BehaviorStrip.h"
#include "Game.h"
#include "dLogger.h"
#include <algorithm>

BehaviorStrip::BehaviorStrip(STRIPID stripID) {
    this->stripID = stripID;
}

BehaviorStrip::~BehaviorStrip() {
    Game::logger->Log("BehaviorState", "Deleting strip %i\n", this->stripID);
    for (auto action : actions) {
        Game::logger->Log("BehaviorState", "Deleting action (%s)\n", action->actionName.c_str());
        delete action;
        action = nullptr;
    }
}

void BehaviorStrip::AddStrip(BehaviorAction* action, double xPosition, double yPosition) {
    this->xPosition = xPosition;
    this->yPosition = yPosition;
    actions.push_back(action);
}

void BehaviorStrip::AddAction(BehaviorAction* action, uint32_t actionIndex) {
    this->actions.insert(this->actions.begin() + actionIndex, action);
}

void BehaviorStrip::RemoveAction(uint32_t actionIndex) {
    auto startAction = actions.begin() + actionIndex;
    auto originalIndex = startAction;
    while (startAction != actions.end()) {
        delete *startAction;
        *startAction = nullptr;
        startAction++;
    }
    this->actions.erase(originalIndex, actions.end());
}

void BehaviorStrip::UpdateUIOfStrip(double xPosition, double yPosition) {
    this->xPosition = xPosition;
    this->yPosition = yPosition;
}

void BehaviorStrip::RearrangeStrip(uint32_t srcActionIndex, uint32_t dstActionIndex) {
    std::rotate(this->actions.begin() + dstActionIndex, this->actions.begin() + srcActionIndex, this->actions.end());
}

void BehaviorStrip::UpdateAction(std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, std::string callbackID, uint32_t actionIndex) {
    auto action = actions.at(actionIndex);

    action->actionName = actionName;
    action->parameterName = parameterName;
    action->parameterValueString = parameterValueString;
    action->parameterValueDouble = parameterValueDouble;
    action->callbackID = callbackID;
}

void BehaviorStrip::SetActions(std::vector<BehaviorAction*> actionsToSet) {
    this->actions = actionsToSet;
}
