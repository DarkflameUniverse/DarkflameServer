#pragma once

#include "BehaviorAction.h"
#include <string>
#include <vector>

class BehaviorStrip {
    public:
        BehaviorStrip(STRIPID stripID);
        ~BehaviorStrip();
        void AddStrip(BehaviorAction* action, double xPosition, double yPosition);
        void AddAction(BehaviorAction* action, uint32_t actionIndex);
        void RemoveAction(uint32_t actionIndex);
        void UpdateUIOfStrip(double xPosition, double yPosition);
        void RearrangeStrip(uint32_t srcActionIndex, uint32_t dstActionIndex);
        void UpdateAction(std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble, std::string callbackID, uint32_t actionIndex);
        void SetActions(std::vector<BehaviorAction*> actionsToSet);
        std::vector<BehaviorAction*> GetActions() { return actions; };
        double GetXPosition() { return xPosition; };
        double GetYPosition() { return yPosition; };
    private:
        std::vector<BehaviorAction*> actions;
        STRIPID stripID;
        double xPosition;
        double yPosition;
};