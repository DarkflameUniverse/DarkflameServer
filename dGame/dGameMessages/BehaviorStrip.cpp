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
        auto modelComponent = modelEntity->GetComponent<ModelComponent>();
        auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
        auto movementAIComponent = modelEntity->GetComponent<MovementAIComponent>();
        NiPoint3 changedAngularVelocity = NiPoint3::ZERO;

        if (!modelComponent || !simplePhysicsComponent || !movementAIComponent) return;

        if (actionToExecute->actionName == "Smash")
        {
            GameMessages::SendSmash(modelEntity, 0.0f, 0.0f, originator->GetObjectID());
            modelComponent->SetSmashedState(true);
            
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
            auto currentVelocity = modelComponent->GetVelocity();
            currentVelocity.y = NiPoint3::UNIT_Y.y;
            changedVelocity = currentVelocity;
            modelComponent->SetVelocity(currentVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "FlyDown")
        {
            auto currentVelocity = modelComponent->GetVelocity();
            currentVelocity.y = NiPoint3::UNIT_Y.y * -1;
            changedVelocity = currentVelocity;
            modelComponent->SetVelocity(currentVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveRight")
        {
            auto currentVelocity = modelComponent->GetVelocity();
            currentVelocity.x = NiPoint3::UNIT_X.x;
            changedVelocity = currentVelocity;
            modelComponent->SetVelocity(currentVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveLeft")
        {
            auto currentVelocity = modelComponent->GetVelocity();
            currentVelocity.x = NiPoint3::UNIT_X.x * -1;
            changedVelocity = currentVelocity;
            modelComponent->SetVelocity(currentVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveForward")
        {
            auto currentVelocity = modelComponent->GetVelocity();
            currentVelocity.z = NiPoint3::UNIT_Z.z;
            changedVelocity = currentVelocity;
            modelComponent->SetVelocity(currentVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "MoveBackward")
        {
            auto currentVelocity = modelComponent->GetVelocity();
            currentVelocity.z = NiPoint3::UNIT_Z.z * -1;
            changedVelocity = currentVelocity;
            modelComponent->SetVelocity(currentVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "Spin")
        {
            auto currentAngularVelocity = modelComponent->GetAngularVelocity();
            currentAngularVelocity.y = NiPoint3::UNIT_Y.y;
            changedAngularVelocity = currentAngularVelocity;
            modelComponent->SetAngularVelocity(currentAngularVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "SpinNegative")
        {
            auto currentAngularVelocity = modelComponent->GetAngularVelocity();
            currentAngularVelocity.y = NiPoint3::UNIT_Y.y * -1;
            changedAngularVelocity = currentAngularVelocity;
            modelComponent->SetAngularVelocity(currentAngularVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "Tilt")
        {
            auto currentAngularVelocity = modelComponent->GetAngularVelocity();
            currentAngularVelocity.z = NiPoint3::UNIT_Z.z;
            changedAngularVelocity = currentAngularVelocity;
            modelComponent->SetAngularVelocity(currentAngularVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "TiltNegative")
        {
            auto currentAngularVelocity = modelComponent->GetAngularVelocity();
            currentAngularVelocity.z = NiPoint3::UNIT_Z.z * -1;
            changedAngularVelocity = currentAngularVelocity;
            modelComponent->SetAngularVelocity(currentAngularVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "Roll")
        {
            auto currentAngularVelocity = modelComponent->GetAngularVelocity();
            currentAngularVelocity.x = NiPoint3::UNIT_X.x;
            changedAngularVelocity = currentAngularVelocity;
            modelComponent->SetAngularVelocity(currentAngularVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
        }
        else if (actionToExecute->actionName == "RollNegative")
        {
            auto currentAngularVelocity = modelComponent->GetAngularVelocity();
            currentAngularVelocity.x = NiPoint3::UNIT_X.x * -1;
            changedAngularVelocity = currentAngularVelocity;
            modelComponent->SetAngularVelocity(currentAngularVelocity);
            timerForNextAction = actionToExecute->parameterValueDouble;
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
            simplePhysicsComponent->SetVelocity(NiPoint3::ZERO);
            simplePhysicsComponent->SetAngularVelocity(NiPoint3::ZERO);
            movementAIComponent->Stop();
            modelComponent->Reset();
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
            modelComponent->MoveTowardsInteractor(originator);
        }
        else if (actionToExecute->actionName == "MoveAwayFromInteractor")
        {
            // RotateByQuaternion?
        }
        else if (actionToExecute->actionName == "MoveBackToStart")
        {
            modelEntity->SetPosition(modelEntity->GetDefaultPosition());
        }
        else if (actionToExecute->actionName == "SetSpeed")
        {
            modelComponent->SetSpeed(actionToExecute->parameterValueDouble);
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

        modelEntity->AddCallbackTimer(timerForNextAction, [changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
            Game::logger->Log("strip", "Ending action (%s)\n", actionToExecute->actionName.c_str());
            auto modelComponent = modelEntity->GetComponent<ModelComponent>();
            if (modelComponent && changedVelocity != NiPoint3::ZERO) {
                auto currentVelocity = modelComponent->GetVelocity();
                currentVelocity = currentVelocity - changedVelocity;
                modelComponent->SetVelocity(currentVelocity);
            }

            if (modelComponent && changedAngularVelocity != NiPoint3::ZERO) {
                auto currentAngularVelocity = modelComponent->GetAngularVelocity();
                currentAngularVelocity = currentAngularVelocity - changedAngularVelocity;
                modelComponent->SetAngularVelocity(currentAngularVelocity);
            }

            EntityManager::Instance()->SerializeEntity(modelEntity);
            if (actionToExecuteIterator == actions.end()) return;
            if (actionToExecute->actionName == "UnSmash") modelComponent->SetSmashedState(false);
            DoAction(actionToExecuteIterator, modelEntity, originator);
        });
}