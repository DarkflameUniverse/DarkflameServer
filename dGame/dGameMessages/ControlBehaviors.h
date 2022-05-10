#pragma once

#include "RakNetDefines.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "dLogger.h"

class ControlBehaviors {
    public:
        ~ControlBehaviors();
        void DoActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner);
    private:
        void SendBehaviorListToClient();
        void ModelTypeChanged();
        void ToggleExecutionUpdates();
        void AddStrip();
        void RemoveStrip();
        void MergeStrips();
        void SplitStrip();
        void UpdateStripUI();
        void AddAction();
        void MigrateActions();
        void RearrangeStrip();
        void Add();
        void RemoveActions();
        void Rename();
        void SendBehaviorBlocksToClient();
        void UpdateAction();
        void MoveToInventory();
        uint32_t GetBehaviorIDFromArgument(std::string key = "BehaviorID");
        BEHAVIORSTATE GetBehaviorStateFromArgument(std::string key = "stateID");
        STRIPID GetStripIDFromArgument(std::string key = "stripID");

        /**
         * The model component belonging to the behavior we are editing
         */
        ModelComponent* modelComponent = nullptr;

        /**
         * The arguments passed to us specifiying information on how to update the behaviors
         */
        AMFArrayValue* arguments = nullptr;

        /**
         * The entity that owns the model we are editing.
         */
        Entity* modelOwner = nullptr;

        /**
         * The system address of the model owner
         */
        SystemAddress sysAddr = UNASSIGNED_SYSTEM_ADDRESS;
};