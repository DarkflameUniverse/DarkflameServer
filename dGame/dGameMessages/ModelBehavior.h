#pragma once

#include <algorithm>
#include <map>
#include <vector>

#include "BehaviorAction.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "ModelComponent.h"
#include "dCommonVars.h"
#include "dLogger.h"
#include "GameMessages.h"
#include "BehaviorState.h"

class ModelComponent;
/**
 * All variable meanings are given in the ModelComponent.h file
 */
class ModelBehavior {
  public:
    ModelBehavior(uint32_t behaviorID, ModelComponent* model, bool isLoot = true, std::string behaviorName = "New Behavior", bool isTemplated = false);
    ~ModelBehavior();
    /**
     * Adds a strip of action(s) to a state.
     */
    void AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName = "", std::string parameterValueString = "", double parameterValueDouble = 0.0,
        std::string callbackID = "", double xPosition = 0.0, double yPosition = 0.0);

    /**
     * Adds an action to an existing strip.
     */
    void AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble,
        std::string callbackID, uint32_t actionIndex);

    /**
     * Removes all actions after the given index from a behvior
     */
    void RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex);

    /**
     * Removes a strip from a state
     */
    void RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID);

    /**
     * Renames the given behavior
     */
    void Rename(std::string newName);

    /**
     * Returns a map of behaviors this component has.
     *
     * @return map of states of behavior actions
     */
    std::map<BEHAVIORSTATE, BehaviorState*> GetBehaviorStates() { return states; };

    /**
     * Updates the UI position of a given strip
     *
     * @param stateID
     * @param stripID
     * @param xPosition The x position of this strip on the UI
     * @param yPosition The y position of this strip on the UI
     * @param behaviorID
     */
    void UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition);

    /**
     * Rearranges a strip
     */
    void RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex);

    /**
     * Migrates actions
     */
    void MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID);

    /**
     * Splits a strip
     */
    void SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, double yPosition, double xPosition);

    /**
     * Merges strips
     */
    void MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t dstActionIndex);

    /**
     * Updates an action
     */
    void UpdateAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString, double parameterValueDouble,
        std::string callbackID, uint32_t actionIndex);

    // Getters

    /**
     * Return the behavior ID of the object
     *
     * @return The behaviors ID
     */
    int32_t GetBehaviorID() { return behaviorID; };

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
     * @brief Verifies that if there is only 1 state with strips in a behavior, that those strips are in the home state.
     *
     */
    void VerifyStates();

    void FindStarterBlocks();

    void OnInteract(Entity* originator); 

    void OnChatMessage(ModelComponent* modelComponent, Entity* originator, std::string& message);

    void SetState(BEHAVIORSTATE stateID);

    void ResetStrips();

    void OnAttack(ModelComponent* modelComponent, Entity* originator);

    void OnStartup(ModelComponent* modelComponent);

    void OnTimer(ModelComponent* modelComponent);

    void SetBehaviorID(uint32_t value) { this->behaviorID = value; this->isLoot = false; };

    void LoadStatesFromXml(tinyxml2::XMLElement* doc);

    bool GetShouldSetNewID() { return shouldSetNewID; };

    void SetShouldGetNewID(bool value) { shouldSetNewID = value; };

    void SetIsTemplated(bool value) { isTemplated = value; };

    bool GetIsTemplated() { return isTemplated; };

  private:
    /**
     * The behavior ID of this behavior
     */
    uint32_t behaviorID;

    /**
     * Whether this behavior is an earned behavior
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
    std::map<BEHAVIORSTATE, BehaviorState*> states;

    /**
     * The model component of the owner
     */
    ModelComponent* m_ModelComponent = nullptr;

    /**
     * The current active state of this behavior
     */
    BEHAVIORSTATE m_ActiveState = eStates::HOME_STATE;

    bool shouldSetNewID = false;
    bool isTemplated = false;
};
