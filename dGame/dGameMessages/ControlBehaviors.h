#pragma once

#include "RakNetDefines.h"
#include "GameMessages.h"
#include "ModelComponent.h"
#include "dLogger.h"
/**
 * Important note regarding behaviors:
 * ALL behaviors MUST be perfectly synced with what the client UI shows.
 * If there is a desync this will cause crashes since
 * the client expects the information to be stored
 * on the server in a specific way.
 */
class ControlBehaviors {
    public:
        /**
         * Destroys the ControlBehaviors object and deletes the arguments array
         */
        ~ControlBehaviors();

        /**
         * Does actions based on the given command
         * 
         * @param modelEntity The Entity that is requesting behavior actions
         * @param sysAddr The SystemAddress of the user who owns this model
         * @param arguments The arguments parsed from the BitStream in the AMF3 format
         * @param command The command to execute
         * @param modelOwner The Entity that owns this model
         */
        void DoActions(Entity* modelEntity, const SystemAddress& sysAddr, AMFArrayValue* arguments, std::string command, Entity* modelOwner);
    private:

        /**
         * Send the list of behaviors the model has to the client
         */
        void SendBehaviorListToClient();

        /**
         * Changes the model type of the model
         */
        void ModelTypeChanged();

        /**
         * Unknown use at the moment.  
         * Only sent when the player edits a behavior (client sends true) 
         * or closes the UI (client sends false).
         */
        void ToggleExecutionUpdates();

        /**
         * Adds a strip to a behavior
         */
        void AddStrip();

        /**
         * Removes a strip from a behavior
         */
        void RemoveStrip();

        /**
         * Merges two strips into one
         */
        void MergeStrips();

        /**
         * Splits a strip into two strips
         */
        void SplitStrip();

        /**
         * Updates the UI position of a strip
         */
        void UpdateStripUI();

        /**
         * Adds an action to a strip
         */
        void AddAction();

        /**
         * Migrates actions from one strip to another
         */
        void MigrateActions();

        /**
         * Rearranges the order of actions within a strip
         */
        void RearrangeStrip();

        /**
         * Adds a behavior to a model
         */
        void Add();

        /**
         * Removes any number of actions (1 or more) from a strip
         */
        void RemoveActions();

        /**
         * Renames a behavior
         */
        void Rename();

        /**
         * Sends the behavior blocks of a behavior to the client
         */
        void SendBehaviorBlocksToClient();

        /**
         * Updates the parameters of an action
         */
        void UpdateAction();

        /**
         * Moves a behavior into the players inventory
         */
        void MoveToInventory();

        /**
         * @brief Gets the behavior id from the arguments given the provided key
         * 
         * @param key a key that references a Behavior id in the arguments
         * @return A Behavior id
         */
        int32_t GetBehaviorIDFromArgument(std::string key = "BehaviorID");

        /**
         * @brief Gets the behavior state from the arguments given the provided key
         * 
         * @param key a key that references a Behavior state in the arguments
         * @return A Behavior state casted to BEHAVIORSTATE
         */
        BEHAVIORSTATE GetBehaviorStateFromArgument(std::string key = "stateID");

        /**
         * @brief Gets the strip id from the arguments given the provided key
         * 
         * @param key a key that references a strip id in the arguments
         * @return A strip id casted to STRIPID
         */
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

        void RequestUpdatedID(int32_t behaviorID);
};