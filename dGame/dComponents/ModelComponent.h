#pragma once
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "dLogger.h"
#include "Component.h"
#include "BehaviorAction.h"
#include "ModelBehavior.h"

#include <map>
#include <vector>

class ModelBehavior;
class Entity;

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class ModelComponent : public Component {
public:
    static const uint32_t ComponentType = COMPONENT_TYPE_MODEL;

    ModelComponent(uint32_t componentID, Entity* parent);
    ~ModelComponent() override;
    void OnUse(Entity* originator) override;
    void Update(float deltatime) override;
    void Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags);

    /**
     * Returns the position of the model
     * @return the position of the model
     */
    NiPoint3& GetPosition() { return m_Position; }

    /**
     * Sets the position of the model
     * @param pos the position to set
     */
    void SetPosition(const NiPoint3& pos) { m_Position = pos; }

    /**
     * Returns the rotation of the model
     * @return the rotation of the model
     */
    NiQuaternion& GetRotation() { return m_Rotation; }

    /**
     * Sets the rotation of the model
     * @param rot the rotation to set
     */
    void SetRotation(const NiQuaternion& rot) { m_Rotation = rot; }

    /**
     * @brief Adds a strip of action(s) to a behavior.  Note that this method itself doesn't add multiple actions rather the message
     * the client sends can add multiple actions to a strip with this one message (for example moving a strip of actions between states).
     * 
     * @param stateID The state id we are adding the strip to
     * @param stripID The strip id we are adding the action(s) to
     * @param actionName The name of the action being added
     * @param behaviorID The behavior id we are adding the strip to.  Note this is passed by reference so if we are creating a new behavior, we can update 
     * the behavior id of the editor on the client.
     * @param parameterName The name of the parameter this action can do.  This will be an empty string if the action has no parameters 
     * @param parameterValueString The string value of the parameter of this action.  This will be an empty string if there is no parameter
     * @param parameterValueDouble The double value of the parameter of this action.  This will be 0.0 if there is no parameter
     * @param callbackID Unknown use as of 05/10/2022
     * @param xPosition The x position of this strip on the UI
     * @param yPosition The y position of this strip on the UI
     */
    void AddStrip(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, uint32_t &behaviorID, std::string parameterName = "", std::string parameterValueString = "",
        double parameterValueDouble = 0.0, std::string callbackID = "", double xPosition = 0.0, double yPosition = 0.0);

    /**
     * @brief Adds an action to a strip
     * 
     * @param stateID The state id we are adding the action to
     * @param stripID The strip id we are adding the action to
     * @param actionName The name of the action being added
     * @param behaviorID The behavior id we are adding the strip to.  Note this is passed by reference so if we are creating a new behavior, we can update 
     * the behavior id of the editor on the client.
     * @param parameterName The name of the parameter this action can do.  This will be an empty string if the action has no parameters 
     * @param parameterValueString The string value of the parameter of this action.  This will be an empty string if there is no parameter
     * @param parameterValueDouble The double value of the parameter of this action.  This will be 0.0 if there is no parameter
     * @param callbackID Unknown use as of 05/10/2022
     * @param actionIndex The index to add the action to
     * @param behaviorID The behavior id to add the action to
     */
    void AddAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString,
        double parameterValueDouble, std::string callbackID, uint32_t actionIndex, uint32_t behaviorID);

    /**
     * @brief Removes an action from a strip
     * 
     * @param stateID The state id we are removing the action from
     * @param stripID The strip id we are removing the action from
     * @param actionIndex The index to add the action to
     * @param behaviorID The behavior id to add the action to
     */
    void RemoveAction(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t actionIndex, uint32_t behaviorID);

    /**
     * @brief Removes a strip from a behavior
     * 
     * @param stateID The state id we are removing the strip from
     * @param stripID The strip id we are removing the strip from
     * @param behaviorID The behavior id to add the action to
     */
    void RemoveStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t behaviorID);

    /**
     * @brief Adds a behavior to a model
     * 
     * @param behaviorID The behavior id to add to the model
     * @param behaviorIndex The (optional) index to add this behavior to
     */
    void AddBehavior(uint32_t behaviorID, uint32_t behaviorIndex);
	
    /**
     * @brief Moves a behavior to the player inventory and removes it from the model 
     * 
     * @param behaviorID The behavior id to remove from the model
     * @param behaviorIndex The (optional) index to remove the behavior from
     */
    void MoveBehaviorToInventory(uint32_t behaviorID, uint32_t behaviorIndex);
    
    /**
     * @brief Renames a behavior
     * 
     * @param behaviorID The behavior id to rename
     * @param newName The new name of the behavior
     */
    void Rename(uint32_t behaviorID, std::string newName);

    /**
     * @brief Updates the UI position of a strip
     * 
     * @param stateID The state id we are updating the UI of
     * @param stripID The strip id we are updating the UI of
     * @param xPosition The x position of this strip on the UI
     * @param yPosition The y position of this strip on the UI
     * @param behaviorID The behavior id to update the UI of
     */
    void UpdateUIOfStrip(BEHAVIORSTATE stateID, STRIPID stripID, double xPosition, double yPosition, uint32_t behaviorID);

    /**
     * @brief Rearranges tha actions in a single strip
     * 
     * @param stateID The state id we are rearranging
     * @param stripID The strip id we are rearranging
     * @param srcActionIndex The source action index we are moving the elements from
     * @param dstActionIndex The destination action index we are moveing the elements to
     * @param behaviorID The behavior id to add the action to
     */
    void RearrangeStrip(BEHAVIORSTATE stateID, STRIPID stripID, uint32_t srcActionIndex, uint32_t dstActionIndex, uint32_t behaviorID);

    /**
     * @brief Migrates actions from one strip to another.
     * 
     * @param srcActionIndex The source actions' index.  Note that this moves every action after the current one, until the end of the strip.
     * @param srcStripID The source strip id
     * @param srcStateID The source state id
     * @param dstActionIndex The destination actions' index
     * @param dstStripID The destination strip id
     * @param dstStateID The destination state id
     * @param behaviorID The behavior id to act on
     */
    void MigrateActions(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, uint32_t dstActionIndex, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID);

    /**
     * @brief Splits a strip into two strips
     * 
     * @param srcActionIndex The source actions' index
     * @param srcStripID The source strip id
     * @param srcStateID The source state id
     * @param dstStripID The destination strip id
     * @param dstStateID The destination state id
     * @param behaviorID The behavior id to add the action to
     * @param yPosition The y position of this strip on the UI
     * @param xPosition The x position of this strip on the UI
     */
    void SplitStrip(uint32_t srcActionIndex, STRIPID srcStripID, BEHAVIORSTATE srcStateID, STRIPID dstStripID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, double yPosition, double xPosition);

    /**
     * @brief Merges two strips together.  
     * 
     * @param srcStripID The source strip id
     * @param dstStripID The destination strip id
     * @param srcStateID The source state id
     * @param dstStateID The destination state id
     * @param behaviorID The behavior id 
     * @param dstActionIndex The destination action index
     */
    void MergeStrips(STRIPID srcStripID, STRIPID dstStripID, BEHAVIORSTATE srcStateID, BEHAVIORSTATE dstStateID, uint32_t behaviorID, uint32_t dstActionIndex);

    /**
     * @brief Updates an actions' parameters
     * 
     * @param stateID The state id
     * @param stripID The strip id
     * @param actionName The action name
     * @param parameterName The action parameter
     * @param parameterValueString The action parameters string value
     * @param parameterValueDouble The action parameters double value
     * @param callbackID Unknown
     * @param actionIndex The actions index
     * @param behaviorID The behavior id
     */
    void UpdateAction(
        BEHAVIORSTATE stateID, STRIPID stripID, std::string actionName, std::string parameterName, std::string parameterValueString,
        double parameterValueDouble, std::string callbackID, uint32_t actionIndex, uint32_t behaviorID);

    /**
     * @brief Set the Model Type object
     * 
     * @param value 
     */
    void SetModelType (uint32_t value) { this->m_ModelType = value; };
    
    /**
     * @brief Get the Behaviors object
     * 
     * @return std::vector<ModelBehavior*> 
     */
    std::vector<ModelBehavior*> GetBehaviors() { return behaviors; };

    void SetBehaviors(std::vector<ModelBehavior*> behaviors) { this->behaviors = behaviors; };

    /**
     * @brief 
     * 
     * @param behaviorID 
     * @return ModelBehavior* 
     */
    ModelBehavior* FindBehavior(uint32_t& behaviorID);

    void SetOnStartup(bool value) { this->onStartup = value; };
    void SetOnAttack(bool value) { this->onAttack = value; };
    void SetOnInteract(bool value) { this->onInteract = value; m_IsPickable = value;};
    void SetOnProximityEnter(bool value) { this->onProximityEnter = value; };
    void SetOnProximityLeave(bool value) { this-> onProximityLeave = value; };
    void SetOnImpact(bool value) { this->onImpact = value; };
    void SetOnChatMessage(bool value) { this->onChatMessage = value; };
    void SetOnTimer(bool value) { this->onTimer = value; };
    void Reset() { onStartup = false;
                    onAttack = false;
                    onInteract = false;
                    onProximityEnter = false;
                    onProximityLeave = false;
                    onImpact = false;
                    onChatMessage = false;
                    onTimer = false; 
                    m_IsPickable = false;
                    interactor = nullptr;
                    moveTowardsInteractor = false;
                    EntityManager::Instance()->SerializeEntity(m_Parent); 
                    m_ResetOnNextUpdate = true;
                    m_MoveSpeed = 1.0f; 
                    velocityDirection = NiPoint3::ZERO; 
                    isMoving = false; 
                    angularVelocityDirection = NiPoint3::ZERO; };
    void MoveTowardsInteractor(Entity* interactor);
    bool GetIsPaused() { return m_IsPaused; };
    void PauseModels() { this->m_IsPaused = true; Reset(); };
    void StartModel() { this->m_IsPaused = false; totalDelta = 10.0f; };
    void SetSmashedState(bool value) { this->m_Smashed = value; };
    void SetSpeed(float value) { this->m_MoveSpeed = value; };
    float GetSpeed() { return m_MoveSpeed; };
    NiPoint3 GetVelocity() { return velocityDirection; };
    void SetVelocity(NiPoint3& value) { this->velocityDirection = value; };
    void SetIsMoving(bool value) { this->isMoving = value; };
    bool GetIsMoving() { return this->isMoving; };
    void SetAngularVelocity(NiPoint3& value) { this->angularVelocityDirection = value; };
    NiPoint3 GetAngularVelocity() { return this->angularVelocityDirection; };
private:

    /**
     * The position of the model
     */
    NiPoint3 m_Position;

    /**
     * The rotation of the model
     */
    NiQuaternion m_Rotation;

    /**
     * The ID of the user that made the model
     */
    LWOOBJID m_userModelID;

    /**
     * A vector of behaviors this component has
     */
    std::vector<ModelBehavior*> behaviors = {};

    /**
     * The models type - leaving this as two for now.
     */
    uint32_t m_ModelType = 2;

    /**
     * Whether or not this entity can be interacted with
     */
    bool m_IsPickable = false;

    /**
     * Whether or not this entity is paused
     */
    bool m_IsPaused = false;

    bool onStartup = false;
    bool onAttack = false;
    bool onInteract = false;
    bool onProximityEnter = false;
    bool onProximityLeave = false;
    bool onImpact = false;
    bool onChatMessage = false;
    bool onTimer = false;
    float totalDelta = 0.0f;
    float secondDelta = 0.0f;
    bool moveTowardsInteractor = false;
    Entity* interactor = nullptr;
    bool m_ResetOnNextUpdate = false;
    bool m_Smashed = false;
    float m_MoveSpeed = 1.0f;
    NiPoint3 velocityDirection = NiPoint3::ZERO;
    bool isMoving = false;
    NiPoint3 angularVelocityDirection = NiPoint3::ZERO;
};