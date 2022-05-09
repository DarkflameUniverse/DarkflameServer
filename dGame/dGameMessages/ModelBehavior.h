#pragma once

#include <map>
#include <vector>
#include "Actions.h"
#include "dCommonVars.h"

class ModelBehavior {
    public:
            ModelBehavior(uint32_t behaviorID = 0);
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
         * Returns a map of behaviors this component has.
         * @return map of states of behavior actions
         */
        std::map<BEHAVIORSTATE, std::map<STRIPID, std::vector<BehaviorAction*>>> GetBehaviorActions() { return states; };

        /**
         * @brief Return the behavior ID of the object
         * 
         * @return The behaviors ID
         */
        uint32_t GetBehaviorID() { return behaviorID; };

        /**
         * @brief Get if the behavior is lootable or not?
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
         * @brief Get the name of this behavior
         * 
         * @return The name of this behavior
         */
        std::string GetName() { return behaviorName; };
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