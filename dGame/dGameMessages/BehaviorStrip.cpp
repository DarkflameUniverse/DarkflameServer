#include "BehaviorStrip.h"
#include "Game.h"
#include "dLogger.h"
#include "SimplePhysicsComponent.h"
#include "GameMessages.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "ModelComponent.h"
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

void BehaviorStrip::ExecuteStrip(ModelComponent* modelComponent, Entity* originator) {
    Game::logger->Log("BehaviorStrip", "Executing strip %i!\n", this->stripID);
    auto modelEntity = modelComponent->GetParent();

    modelEntity->AddCallbackTimer(0.0f, [modelEntity, originator, this]() {
        auto actionToExecuteIterator = this->actions.begin() + 1; // Start at action after starter block
        this->DoAction(actionToExecuteIterator, modelEntity, originator);
    });
}

void BehaviorStrip::DoAction(std::vector<BehaviorAction *>::iterator actionToExecuteIterator, Entity* modelEntity, Entity* originator) {
        auto actionToExecute = *actionToExecuteIterator;
        float timerForNextAction = 0.0f;

        Game::logger->Log("BehaviorStrip", "Executing action (%s)\n", actionToExecute->actionName.c_str());

        if (actionToExecute->actionName == "Smash")
        {
            GameMessages::SendSmash(modelEntity, 0.0f, 0.0f, originator->GetObjectID());
        }
        else if (actionToExecute->actionName == "UnSmash")
        {
            GameMessages::SendUnSmash(modelEntity, LWOOBJID_EMPTY, actionToExecute->parameterValueDouble);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "Wait")
        {
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "FlyUp")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::UNIT_Y);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "FlyDown")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::UNIT_Y * - 1);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveRight")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::UNIT_X);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveLeft")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::UNIT_X * - 1);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveForward")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::UNIT_Z);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveBackward")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::UNIT_Z * - 1);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "Spin")
        {

        }
        else if (actionToExecute->actionName == "SpinNegative")
        {

        }
        else if (actionToExecute->actionName == "Tilt")
        {

        }
        else if (actionToExecute->actionName == "TiltNegative")
        {

        }
        else if (actionToExecute->actionName == "Roll")
        {

        }
        else if (actionToExecute->actionName == "RollNegative")
        {

        }
        else if (actionToExecute->actionName == "SpawnStromling")
        {

        }
        else if (actionToExecute->actionName == "SpawnPirate")
        {

        }
        else if (actionToExecute->actionName == "SpawnRonin")
        {

        }
        else if (actionToExecute->actionName == "DoDamage")
        {

        }
        else if (actionToExecute->actionName == "DropArmor")
        {

        }
        else if (actionToExecute->actionName == "DropHealth")
        {

        }
        else if (actionToExecute->actionName == "DropImagination")
        {

        }
        else if (actionToExecute->actionName == "Restart")
        {

        }
        else if (actionToExecute->actionName == "PrivateMessage")
        {

        }
        else if (actionToExecute->actionName == "Chat")
        {

        }
        else if (actionToExecute->actionName == "PlaySound")
        {

        }
        else if (actionToExecute->actionName == "MoveToInteractor")
        {

        }
        else if (actionToExecute->actionName == "MoveAwayFromInteractor")
        {

        }
        else if (actionToExecute->actionName == "MoveBackToStart")
        {

        }
        else if (actionToExecute->actionName == "SetSpeed")
        {

        }
        else if (actionToExecute->actionName == "ChangeStateHome")
        {

        }
        else if (actionToExecute->actionName == "ChangeStateCircle")
        {

        }
        else if (actionToExecute->actionName == "ChangeStateSquare")
        {

        }
        else if (actionToExecute->actionName == "ChangeStateDiamond")
        {

        }
        else if (actionToExecute->actionName == "ChangeStateTriangle")
        {

        }
        else if (actionToExecute->actionName == "ChangeStateStar")
        {

        }
        EntityManager::Instance()->SerializeEntity(modelEntity);
        actionToExecuteIterator++;

        modelEntity->AddCallbackTimer(timerForNextAction, [actionToExecuteIterator, modelEntity, originator, this]() {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(NiPoint3::ZERO);
            simplePhysicsComponent->SetAngularVelocity(NiPoint3::ZERO);
             EntityManager::Instance()->SerializeEntity(modelEntity);
            if (actionToExecuteIterator == actions.end()) return;
            DoAction(actionToExecuteIterator, modelEntity, originator);
        });
}