#pragma once

#include <map>
#include <vector>
#include "Actions.h"
#include "dCommonVars.h"

class ModelBehavior {
    public:
            ModelBehavior(uint32_t behaviorID = 0, bool isLoot = true);
            ~ModelBehavior();
        /**
         * Adds a strip of action(s) to a state.
         */
        void AddStrip(
            BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName = "", std::string actionParameter = "", double actionParameterValue = 0.0,
            std::string callbackID = "", double xPosition = 0.0, double yPosition = 0.0, uint32_t behaviorID = 0, std::string behaviorName = "");

        /**
         * Adds an action to an existing strip.
         */
        void AddAction(
            BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
	    	std::string callbackID, uint32_t actionIndex, uint32_t behaviorID);

        /**
         * Removes all actions after the given index from a behvior
         */
        void RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, uint32_t behaviorID);

        /**
         * Removes a strip from a state
         */
        void RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t behaviorID);

        /**
         * Renames the given behavior
         */
        void Rename(uint32_t behaviorID, std::string newName);

        /**
         * Returns a map of behaviors this component has.
         * 
         * @return map of states of behavior actions
         */
        std::map<BEHAVIORSTATE, std::map<STRIPID, std::vector<BehaviorAction*>>> GetBehaviorActions() { return states; };

        /**
         * Return the behavior ID of the object
         * 
         * @return The behaviors ID
         */
        uint32_t GetBehaviorID() { return behaviorID; };

        /**
         * Get if the behavior is lootable or not?
         * 
         * @return If the loot is enabled
         */
        bool GetIsLoot() { return isLoot; };

        /**
         * Gets the locked state of the behavior
         * 
         * @return Whether the behavior is locked or not
         */
        bool GetIsLocked() { return isLocked; };

        /**
         * Get the name of this behavior
         * 
         * @return The name of this behavior
         */
        std::string GetName() { return behaviorName; };

        /**
         * Updates the UI position of a given strip
         * 
         * @param stateID 
         * @param stripID 
         * @param xPosition 
         * @param yPosition 
         * @param behaviorID 
         */
        void UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition, uint32_t behaviorID);

        /**
         * Rearranges a strip
         */
        void RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex, uint32_t behaviorID);

        /**
         * Migrates actions
         */
        void MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID); 

        /**
         * Splits a strip
         */
        void SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, double yPosition, double xPosition);

        /**
         * Merges strips
         */
        void MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, uint32_t dstActionIndex);

        /**
         * Updates an action
         */
        void UpdateAction(
            BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string actionParameter, double actionParameterValue, 
	    	std::string callbackID, uint32_t actionIndex, uint32_t behaviorID);

        /**
         * Sets the locked state
         */
        void SetLockState(bool value) { this->isLocked = value; };
    private:
        /**
         * The behavior ID of this behavior
         */
        uint32_t behaviorID;

        /**
         * Whether the model drops loot?
         */
        bool isLoot;

        /**
         * Whether the model is unlocked?
         */
        bool isLocked;

        /**
         * The name of this behavior
         */
        std::string behaviorName;

        /**
         * A map representing the behaviors this model has
         */
        std::map<BEHAVIORSTATE, std::map<STRIPID, std::vector<BehaviorAction*>>> states = {
                {eStates::HOME_STATE, {}},
                {eStates::CIRCLE_STATE, {}},
                {eStates::SQUARE_STATE, {}},
                {eStates::DIAMOND_STATE, {}},
                {eStates::TRIANGLE_STATE, {}},
                {eStates::STAR_STATE, {}}
            };
};