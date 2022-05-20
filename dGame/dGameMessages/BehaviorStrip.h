#pragma once

#include "Entity.h"
#include "BehaviorAction.h"
#include <string>
#include <vector>
class ModelComponent;
class ModelBehavior;
class Entity;
class BehaviorStrip {
    public:
        BehaviorStrip(STRIPID stripID, ModelBehavior* behavior);
        ~BehaviorStrip();
        void AddStrip(BehaviorAction* action, double xPosition, double yPosition);
        void AddAction(BehaviorAction* action, uint32_t actionIndex);
        void RemoveAction(uint32_t actionIndex);
        void UpdateUIOfStrip(double xPosition, double yPosition);
        void RearrangeStrip(uint32_t srcActionIndex, uint32_t dstActionIndex);
        void UpdateAction(std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, std::string callbackID, uint32_t actionIndex);
        void SetActions(std::vector<BehaviorAction*> actionsToSet);
        void ExecuteStrip(ModelComponent* modelComponent, Entity* originator);
        void DoAction(std::vector<BehaviorAction *>::iterator actionToExecuteIterator, Entity* modelEntity, Entity* originator);
        std::vector<BehaviorAction*> GetActions() { return actions; };
        double GetXPosition() { return xPosition; };
        double GetYPosition() { return yPosition; };
        bool IsActive() { return isActive; };
        void SetIsActive(bool value) { this->isActive = value; };
    private:
        ModelBehavior* parentBehavior = nullptr;
        std::vector<BehaviorAction*> actions;
        STRIPID stripID;
        double xPosition;
        double yPosition;
        bool isActive = false;
};