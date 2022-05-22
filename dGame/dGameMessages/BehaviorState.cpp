#include "BehaviorState.h"
#include "Game.h"
#include "ModelComponent.h"
#include "dLogger.h"

BehaviorState::BehaviorState(BEHAVIORSTATE stateID) {
    this->stateID = stateID;
}

BehaviorState::~BehaviorState() {
    Game::logger->Log("BehaviorState", "Deleting state %i\n", this->stateID);
    for (auto strip : strips) {
        delete strip.second;
        strip.second = nullptr;
    }
}

void BehaviorState::AddStrip(BehaviorAction* action, STRIPID stripID, double xPosition, double yPosition, ModelBehavior* behavior) {
    // Find the strip
    auto strip = strips.find(stripID);
    // Should it be a new strip create it and add this element to it
    if (strip == strips.end()) {
       auto newStrip = new BehaviorStrip(stripID, behavior);
       newStrip->AddStrip(action, xPosition, yPosition);
       strips.insert(std::make_pair(stripID, newStrip));
    } else {
        strip->second->AddStrip(action, xPosition, yPosition);
    }
}

void BehaviorState::AddAction(BehaviorAction* action, STRIPID stripID, uint32_t actionIndex) {
    auto strip = strips.find(stripID);

    strip->second->AddAction(action, actionIndex);
}

void BehaviorState::RemoveAction(STRIPID stripID, uint32_t actionIndex) {
    auto strip = strips.find(stripID);

    strip->second->RemoveAction(actionIndex);
}

void BehaviorState::RemoveStrip(STRIPID stripID) {
    auto strip = strips.find(stripID);
    
    delete strip->second;
    strip->second = nullptr;

    strips.erase(stripID);
}

void BehaviorState::UpdateUIOfStrip(STRIPID stripID, double xPosition, double yPosition) {
    auto strip = strips.find(stripID);

    strip->second->UpdateUIOfStrip(xPosition, yPosition);
}

void BehaviorState::RearrangeStrip(STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex) {
    auto strip = strips.find(stripID)->second;

    strip->RearrangeStrip(srcActionIndex, dstActionIndex);
}

void BehaviorState::MigrateActions(BehaviorState* srcState, uint32_t srcActionIndex, STRIPID srcStripID, uint32_t dstActionIndex, STRIPID dstStripID) {
    // Find the source strip and its actions
    auto srcStrip = srcState->GetStripByID(srcStripID);
    auto srcStripActions = srcStrip->GetActions();

    // Find the destination strip and its actions
    auto dstStrip = strips.find(dstStripID)->second;
    auto dstStripActions = dstStrip->GetActions();
    Game::logger->Log("BehaviorState", "Size of strips src %i dst %i\n", srcStripActions.size(), dstStripActions.size());
    // Insert the source actions into the destination actions vector and set this to be the new list of actions
    dstStripActions.insert(dstStripActions.begin() + dstActionIndex, srcStripActions.begin() + srcActionIndex, srcStripActions.end());
    dstStrip->SetActions(dstStripActions);

    // Erase, but not delete, the actions from the source vector so they no longer exist there
    srcStripActions.erase(srcStripActions.begin() + srcActionIndex, srcStripActions.end());
    srcStrip->SetActions(srcStripActions);
}

void BehaviorState::SplitStrip(BehaviorState* srcState, uint32_t srcActionIndex, STRIPID srcStripID, STRIPID dstStripID, double yPosition, double xPosition, ModelBehavior* behavior) {
    // Find the source strip and its actions
    auto srcStrip = srcState->GetStripByID(srcStripID);
    auto srcStripActions = srcStrip->GetActions();

    BehaviorStrip* newStrip = new BehaviorStrip(dstStripID, behavior);
    newStrip->UpdateUIOfStrip(xPosition, yPosition);

    std::vector<BehaviorAction*> newStripActions;
    newStripActions.insert(newStripActions.begin(), srcStripActions.begin() + srcActionIndex, srcStripActions.end());
    newStrip->SetActions(newStripActions);
    this->strips.insert(std::make_pair(dstStripID, newStrip));

    srcStripActions.erase(srcStripActions.begin() + srcActionIndex, srcStripActions.end());
    srcStrip->SetActions(srcStripActions);
}

void BehaviorState::MergeStrips(BehaviorState* srcState, STRIPID srcStripID, STRIPID dstStripID, uint32_t dstActionIndex) {
    // Find the source strip and its actions
    auto srcStrip = srcState->GetStripByID(srcStripID);
    auto srcStripActions = srcStrip->GetActions();

    // Find the destination strip and its actions
    auto dstStrip = strips.find(dstStripID)->second;
    auto dstStripActions = dstStrip->GetActions();

    dstStripActions.insert(dstStripActions.begin() + dstActionIndex, srcStripActions.begin(), srcStripActions.end());
    dstStrip->SetActions(dstStripActions);

    strips.erase(srcStripID);
}

void BehaviorState::UpdateAction(STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, std::string callbackID, uint32_t actionIndex) {
    auto strip = strips.find(stripID);

    strip->second->UpdateAction(actionName, parameterName, parameterValueString, parameterValueDouble, callbackID, actionIndex);
}

BehaviorStrip* BehaviorState::GetStripByID(STRIPID stripID) {
    return strips.find(stripID)->second;
}

void BehaviorState::FindStarterBlocks(ModelComponent* modelComponent) {
    for (auto strip : strips) {
        if (strip.second->IsActive()) break;
        if (strip.second->GetActions().size() == 0) break;
        auto starterBlock = strip.second->GetActions().at(0);
        if (starterBlock->actionName == "OnInteract") modelComponent->SetOnInteract(true);
        else if (starterBlock->actionName == "OnAttack") modelComponent->SetOnAttack(true);
        else if (starterBlock->actionName == "OnEnterProximity") modelComponent->SetOnProximityEnter(true);
        else if (starterBlock->actionName == "OnLeaveProximity") modelComponent->SetOnProximityLeave(true);
        else if (starterBlock->actionName == "OnImpact") modelComponent->SetOnImpact(true);
        else if (starterBlock->actionName == "OnChat") modelComponent->SetOnChatMessage(true);
        else if (starterBlock->actionName == "OnTimer") modelComponent->SetOnTimer(true);
        else if (starterBlock->actionName == "OnStartup") modelComponent->SetOnStartup(true);
    }
}

void BehaviorState::OnInteract(ModelComponent* modelComponent, Entity* originator) {
    for (auto strip : strips) {
        if (strip.second->GetActions().at(0)->actionName == "OnInteract" && !strip.second->IsActive()) strip.second->ExecuteStrip(modelComponent, originator);
    }
}

void BehaviorState::OnChatMessage(ModelComponent* modelComponent, Entity* originator, std::string& message) {
    bool hasMoreChatTriggers = false;
    for (auto strip : strips) {
        auto actions = strip.second->GetActions();
        if (actions.size() > 0 && actions.at(0)->actionName == "OnChat") {
            if (actions.at(0)->parameterValueString == message && !strip.second->IsActive()) {
                Game::logger->Log("BehaviorState", "Executing action %s with value %s\n", actions.at(0)->actionName.c_str(), actions.at(0)->parameterValueString.c_str());
                strip.second->ExecuteStrip(modelComponent, originator);
            } else if (!strip.second->IsActive()){
                hasMoreChatTriggers = true;
            }
        }
    }
    Game::logger->Log("BehaviorState", "Does model have available triggers? %i\n", hasMoreChatTriggers);
    modelComponent->SetOnChatMessage(hasMoreChatTriggers);
}

void BehaviorState::ResetStrips() {
    for (auto strip : strips) {
        Game::logger->Log("BehaviorState", "Setting strip %i in state %i to false\n", strip.first, this->stateID);
        strip.second->SetIsActive(false);
    }
}

void BehaviorState::OnAttack(ModelComponent* modelComponent, Entity* originator) {
    for (auto strip : strips) {
        if (strip.second->GetActions().at(0)->actionName == "OnAttack" && !strip.second->IsActive()) strip.second->ExecuteStrip(modelComponent, originator);
    }
}

void BehaviorState::OnStartup(ModelComponent* modelComponent) {
    for (auto strip : strips) {
        if (strip.second->GetActions().at(0)->actionName == "OnStartup" && !strip.second->IsActive()) strip.second->ExecuteStrip(modelComponent, nullptr);
    }
}

void BehaviorState::OnTimer(ModelComponent* modelComponent) {
    for (auto strip : strips) {
        auto action = strip.second->GetActions().at(0);
        if (action->actionName == "OnTimer" && !strip.second->IsActive()) {
            modelComponent->GetParent()->AddCallbackTimer(action->parameterValueDouble, [strip, this, modelComponent](){
                strip.second->ExecuteStrip(modelComponent, nullptr);
            });
        }
    }
}

void BehaviorState::LoadStripsFromXml(tinyxml2::XMLElement* doc, ModelBehavior* parentBehavior) {
    auto stripsInfo = doc->FirstChildElement("Strip");

    while (stripsInfo != nullptr) {
        STRIPID stripID = 0;
        double xPosition = 0.0f;
        double yPosition = 0.0f;

        stripsInfo->QueryAttribute("stripID", &stripID);
        stripsInfo->QueryAttribute("xPosition", &xPosition);
        stripsInfo->QueryAttribute("yPosition", &yPosition);

        auto stripInfo = new BehaviorStrip(stripID, parentBehavior);
        
        stripInfo->UpdateUIOfStrip(xPosition, yPosition);
        stripInfo->LoadActionsFromXml(stripsInfo);

        strips.insert(std::make_pair(stripID, stripInfo));

        stripsInfo = stripsInfo->NextSiblingElement();
    }
}
