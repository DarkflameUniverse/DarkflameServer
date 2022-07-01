#pragma once

#include "BehaviorStrip.h"
#include <map>
class ModelComponent;
class ModelBehavior;
class ModelBehavior;
class BehaviorState {
    public:
        BehaviorState(BEHAVIORSTATE stateID);
        ~BehaviorState();
        void AddStrip(BehaviorAction* action, STRIPID stripID, double xPosition, double yPosition, ModelBehavior* behavior);
        void AddAction(BehaviorAction* action, STRIPID stripID, uint32_t actionIndex);
        void RemoveAction(STRIPID stripID, uint32_t actionIndex);
        void RemoveStrip(STRIPID stripID);
        void UpdateUIOfStrip(STRIPID stripID, double xPosition, double yPosition);
        void RearrangeStrip(STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex);
        void MigrateActions(BehaviorState* srcState, uint32_t srcActionIndex, STRIPID srcStripID, uint32_t dstActionIndex, STRIPID dstStripID);
        void SplitStrip(BehaviorState* srcState, uint32_t srcActionIndex, STRIPID srcStripID, STRIPID dstStripID, double yPosition, double xPosition, ModelBehavior* behavior);
        void MergeStrips(BehaviorState* srcState, STRIPID srcStripID, STRIPID dstStripID, uint32_t dstActionIndex);
        void UpdateAction(STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, std::string callbackID, uint32_t actionIndex);
        BehaviorStrip* GetStripByID(STRIPID stripID);
        void FindStarterBlocks(ModelComponent* modelComponent);
        void OnInteract(ModelComponent* modelComponent, Entity* originator);
        void OnChatMessage(ModelComponent* modelComponent, Entity* originator, std::string& message); 
        void OnAttack(ModelComponent* modelComponent, Entity* originator);
        void OnStartup(ModelComponent* modelComponent);
        void OnTimer(ModelComponent* modelComponent); 
        uint32_t CountOfStrips() { return strips.size(); };
        void ClearStrips() { strips.clear(); };
        void ResetStrips();
        std::map<STRIPID, BehaviorStrip*> GetStrips() { return strips; };
        void LoadStripsFromXml(tinyxml2::XMLElement* doc, ModelBehavior* parentBehavior);
    private:
        std::map<STRIPID, BehaviorStrip*> strips;
        BEHAVIORSTATE stateID;
};