#include "BehaviorStrip.h"
#include "Game.h"
#include "dLogger.h"
#include "SimplePhysicsComponent.h"
#include "GameMessages.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "DestroyableComponent.h"
#include "MovementAIComponent.h"
#include "PropertyManagementComponent.h"
#include "ModelComponent.h"
#include "ChatPackets.h"
#include <algorithm>

BehaviorStrip::BehaviorStrip(STRIPID stripID, ModelBehavior* behavior) {
    this->stripID = stripID;
    this->parentBehavior = behavior;
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

    isActive = true;
    modelEntity->AddCallbackTimer(0.0f, [modelEntity, originator, this]() {
        auto actionToExecuteIterator = this->actions.begin() + 1; // Start at action after starter block
        if (actionToExecuteIterator == this->actions.end()) {
            isActive = false;
            return;
        }
        this->DoAction(actionToExecuteIterator, modelEntity, originator);
    });
}

void BehaviorStrip::DoAction(std::vector<BehaviorAction *>::iterator actionToExecuteIterator, Entity* modelEntity, Entity* originator) {
        auto actionToExecute = *actionToExecuteIterator;
        float timerForNextAction = 0.0f;
        NiPoint3 changedVelocity = NiPoint3::ZERO;
        Game::logger->Log("BehaviorStrip", "Executing action (%s) for behavior %i\n", actionToExecute->actionName.c_str(), parentBehavior->GetBehaviorID());
        auto modelComponent = modelEntity->GetComponent<ModelComponent>();
        auto simplePhysicsComponent = modelEntity->GetComponent<SimplePhysicsComponent>();
        auto movementAIComponent = modelEntity->GetComponent<MovementAIComponent>();
        NiPoint3 changedAngularVelocity = NiPoint3::ZERO;

        if (!modelComponent || !simplePhysicsComponent || !movementAIComponent) return;

        if (actionToExecute->actionName == "Smash")
        {
            GameMessages::SendSmash(modelEntity, 0.0f, 0.0f, originator != nullptr ? originator->GetObjectID() : LWOOBJID_EMPTY);
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
            modelComponent->AddToYDistance(actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "FlyDown")
        {
            modelComponent->AddToYDistance(-actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "MoveRight")
        {
            modelComponent->AddToXDistance(actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "MoveLeft")
        {
            modelComponent->AddToXDistance(-actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "MoveForward")
        {
            modelComponent->AddToZDistance(actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "MoveBackward")
        {
            modelComponent->AddToZDistance(-actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "Spin")
        {
            modelComponent->AddToYRotation(actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "SpinNegative")
        {
            modelComponent->AddToYRotation(-actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "Tilt")
        {
            modelComponent->AddToZRotation(actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "TiltNegative")
        {
            modelComponent->AddToZRotation(-actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "Roll")
        {
            modelComponent->AddToXRotation(actionToExecute->parameterValueDouble);
        }
        else if (actionToExecute->actionName == "RollNegative")
        {
            modelComponent->AddToXRotation(-actionToExecute->parameterValueDouble);
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
            // TODO
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
            movementAIComponent->Stop();
            modelComponent->Reset();
            isActive = false;
            return;
        }
        else if (actionToExecute->actionName == "PrivateMessage")
        {
            PropertyManagementComponent::Instance()->ChatMessageSentByServer(modelEntity, actionToExecute->parameterValueString);
        }
        else if (actionToExecute->actionName == "Chat")
        {
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
            // TODO
        }
        else if (actionToExecute->actionName == "MoveAwayFromInteractor")
        {
            // TODO
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
            modelComponent->CancelAllActions();
            parentBehavior->SetState(eStates::HOME_STATE);
            return;
        }
        else if (actionToExecute->actionName == "ChangeStateCircle")
        {
            modelComponent->CancelAllActions();
            parentBehavior->SetState(eStates::CIRCLE_STATE);
            return;
        }
        else if (actionToExecute->actionName == "ChangeStateSquare")
        {
            modelComponent->CancelAllActions();
            parentBehavior->SetState(eStates::SQUARE_STATE);
            return;
        }
        else if (actionToExecute->actionName == "ChangeStateDiamond")
        {
            modelComponent->CancelAllActions();
            parentBehavior->SetState(eStates::DIAMOND_STATE);
            return;
        }
        else if (actionToExecute->actionName == "ChangeStateTriangle")
        {
            modelComponent->CancelAllActions();
            parentBehavior->SetState(eStates::TRIANGLE_STATE);
            return;
        }
        else if (actionToExecute->actionName == "ChangeStateStar")
        {
            modelComponent->CancelAllActions();
            parentBehavior->SetState(eStates::STAR_STATE);
            return;
        }
        EntityManager::Instance()->SerializeEntity(modelEntity);
        actionToExecuteIterator++;
        if (actionToExecute->actionName == "FlyUp" || actionToExecute->actionName == "FlyDown") {
            modelComponent->AddYPositionCallback([modelComponent, changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "Y Ending action (%s)\n", actionToExecute->actionName.c_str());

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        } else if (actionToExecute->actionName == "MoveLeft" || actionToExecute->actionName == "MoveRight") {
            modelComponent->AddXPositionCallback([modelComponent, changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "X Ending action (%s)\n", actionToExecute->actionName.c_str());

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        } else if (actionToExecute->actionName == "MoveForward" || actionToExecute->actionName == "MoveBackward") {
            modelComponent->AddZPositionCallback([modelComponent, changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "Z Ending action (%s)\n", actionToExecute->actionName.c_str());

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        } else if (actionToExecute->actionName == "Spin" || actionToExecute->actionName == "SpinNegative") {
            modelComponent->AddYRotationCallback([modelComponent, changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "Y rotation Ending action (%s)\n", actionToExecute->actionName.c_str());

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        } else if (actionToExecute->actionName == "Tilt" || actionToExecute->actionName == "TiltNegative") {
            modelComponent->AddZRotationCallback([modelComponent, changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "Z rotation Ending action (%s)\n", actionToExecute->actionName.c_str());

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        } else if (actionToExecute->actionName == "Roll" || actionToExecute->actionName == "RollNegative") {
            modelComponent->AddXRotationCallback([modelComponent, changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "X rotation Ending action (%s)\n", actionToExecute->actionName.c_str());

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        } else {
            modelEntity->AddCallbackTimer(timerForNextAction, [changedVelocity, actionToExecuteIterator, actionToExecute, modelEntity, originator, this, changedAngularVelocity]() {
                Game::logger->Log("strip", "Ending action (%s)\n", actionToExecute->actionName.c_str());
                auto modelComponent = modelEntity->GetComponent<ModelComponent>();

                EntityManager::Instance()->SerializeEntity(modelEntity);
                if (actionToExecute->actionName == "UnSmash") modelComponent->SetSmashedState(false);
                if (actionToExecuteIterator == actions.end()) {
                    if (this->actions.at(0)->actionName == "OnTimer") {
                        SetIsActive(false);
                        modelComponent->GetParent()->AddCallbackTimer(this->actions.at(0)->parameterValueDouble, [modelComponent, this, originator](){
                            ExecuteStrip(modelComponent, originator);
                        });
                    }
                    return;
                }
                DoAction(actionToExecuteIterator, modelEntity, originator);
            });
        }
}