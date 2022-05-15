#include "BehaviorStrip.h"
#include "Game.h"
#include "dLogger.h"
#include "SimplePhysicsComponent.h"
#include "GameMessages.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "DestroyableComponent.h"
#include "MovementAIComponent.h"
#include "ModelComponent.h"
#include "ChatPackets.h"
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
        if (actionToExecuteIterator == this->actions.end()) return;
        this->DoAction(actionToExecuteIterator, modelEntity, originator);
    });
}

void BehaviorStrip::DoAction(std::vector<BehaviorAction *>::iterator actionToExecuteIterator, Entity* modelEntity, Entity* originator) {
        auto actionToExecute = *actionToExecuteIterator;
        float timerForNextAction = 0.0f;
        NiPoint3 changedVelocity = NiPoint3::ZERO;
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
            changedVelocity = NiPoint3::UNIT_Y;
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() + changedVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "FlyDown")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            changedVelocity = NiPoint3::UNIT_Y * - 1;
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() + changedVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveRight")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            changedVelocity = NiPoint3::UNIT_X;
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() + changedVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveLeft")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            changedVelocity = NiPoint3::UNIT_X * - 1;
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() + changedVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveForward")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            changedVelocity = NiPoint3::UNIT_Z;
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() + changedVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveBackward")
        {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            changedVelocity = NiPoint3::UNIT_Z * - 1;
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() + changedVelocity);
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
            EntityInfo info;
            info.lot = 10495;
            info.pos = modelEntity->GetPosition();
            info.rot = modelEntity->GetRotation();
            info.spawnerID = modelEntity->GetObjectID();

            auto enemy = EntityManager::Instance()->CreateEntity(info);

            EntityManager::Instance()->ConstructEntity(enemy);
        }
        else if (actionToExecute->actionName == "SpawnPirate")
        {
            EntityInfo info;
            info.lot = 10497;
            info.pos = modelEntity->GetPosition();
            info.rot = modelEntity->GetRotation();
            info.spawnerID = modelEntity->GetObjectID();

            auto enemy = EntityManager::Instance()->CreateEntity(info);

            EntityManager::Instance()->ConstructEntity(enemy);
        }
        else if (actionToExecute->actionName == "SpawnRonin")
        {
            EntityInfo info;
            info.lot = 10498;
            info.pos = modelEntity->GetPosition();
            info.rot = modelEntity->GetRotation();
            info.spawnerID = modelEntity->GetObjectID();

            auto enemy = EntityManager::Instance()->CreateEntity(info);

            EntityManager::Instance()->ConstructEntity(enemy);
        }
        else if (actionToExecute->actionName == "DoDamage")
        {

        }
        else if (actionToExecute->actionName == "DropArmor")
        {
            auto players = EntityManager::Instance()->GetEntitiesByLOT(1);
            for (auto player : players) {
                for (uint32_t powerupsToDrop = 0; powerupsToDrop < (uint32_t)actionToExecute->parameterValueDouble; powerupsToDrop++) {
                    GameMessages::SendDropClientLoot(player, modelEntity->GetObjectID(), 6431, 0, modelEntity->GetPosition());
                }
            }
        }
        else if (actionToExecute->actionName == "DropHealth")
        {
            auto players = EntityManager::Instance()->GetEntitiesByLOT(1);
            for (auto player : players) {
                for (uint32_t powerupsToDrop = 0; powerupsToDrop < (uint32_t)actionToExecute->parameterValueDouble; powerupsToDrop++) {
                    GameMessages::SendDropClientLoot(player, modelEntity->GetObjectID(), 177, 0, modelEntity->GetPosition());
                }
            }
        }
        else if (actionToExecute->actionName == "DropImagination")
        {
            auto players = EntityManager::Instance()->GetEntitiesByLOT(1);
            for (auto player : players) {
                for (uint32_t powerupsToDrop = 0; powerupsToDrop < (uint32_t)actionToExecute->parameterValueDouble; powerupsToDrop++) {
                    GameMessages::SendDropClientLoot(player, modelEntity->GetObjectID(), 935, 0, modelEntity->GetPosition());
                }
            }
        }
        else if (actionToExecute->actionName == "Restart")
        {
            modelEntity->SetPosition(modelEntity->GetDefaultPosition());
            modelEntity->SetRotation(modelEntity->GetDefaultRotation());
            modelEntity->GetComponent<SimplePhysicsComponent>()->SetVelocity(NiPoint3::ZERO);
            modelEntity->GetComponent<SimplePhysicsComponent>()->SetAngularVelocity(NiPoint3::ZERO);
            modelEntity->GetComponent<MovementAIComponent>()->Stop();
            modelEntity->GetComponent<ModelComponent>()->Reset();
        }
        else if (actionToExecute->actionName == "PrivateMessage")
        {

        }
        else if (actionToExecute->actionName == "Chat")
        {
            // Rule Board needs to be replaced with the models name!
            ChatPackets::SendChatMessage(UNASSIGNED_SYSTEM_ADDRESS, 12, "REMEMBER TO NAME MODELS", modelEntity->GetObjectID(), false, GeneralUtils::ASCIIToUTF16(actionToExecute->parameterValueString));
        }
        else if (actionToExecute->actionName == "PlaySound")
        {
            auto result = CDClientDatabase::ExecuteQuery("SELECT guid FROM UGBehaviorSounds WHERE id = " + std::to_string((uint32_t)actionToExecute->parameterValueDouble) + ";");
            std::string audioGUID = result.getStringField(0, "");
            GameMessages::SendPlayNDAudioEmitter(modelEntity, UNASSIGNED_SYSTEM_ADDRESS, audioGUID);
        }
        else if (actionToExecute->actionName == "MoveToInteractor")
        {
            modelEntity->GetComponent<ModelComponent>()->MoveTowardsInteractor(originator);
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

        modelEntity->AddCallbackTimer(timerForNextAction, [changedVelocity, actionToExecuteIterator, modelEntity, originator, this]() {
            auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
            simplePhysicsComponent->SetVelocity(simplePhysicsComponent->GetVelocity() - changedVelocity);
            EntityManager::Instance()->SerializeEntity(modelEntity);
            if (actionToExecuteIterator == actions.end()) return;
            DoAction(actionToExecuteIterator, modelEntity, originator);
        });
}