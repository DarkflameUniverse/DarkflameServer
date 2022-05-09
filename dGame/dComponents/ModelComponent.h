#pragma once
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"
#include "Actions.h"
#include "ModelBehavior.h"

#include <map>
#include <vector>

class Entity;

/**
 * Component that represents entities that are a model, e.g. collectible models and BBB models.
 */
class ModelComponent : public Component {
public:
    static const uint32_t ComponentType = COMPONENT_TYPE_MODEL;

    ModelComponent(uint32_t componentID, Entity* parent);
    ~ModelComponent() override;

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
     * Adds a behavior to the vector of behaviors
     * 
     * @param behaviorID Behavior ID to add
     * @param behaviorIndex Index this behavior is located at
     */
    void AddBehavior(uint32_t behaviorID, uint32_t behaviorIndex);
    
    /**
     * @brief Returns the behaviors this model has
     * 
     * @return a vector of behaviors
     */
    std::vector<ModelBehavior*> GetBehaviors() { return behaviors; };

    /**
     * Finds a behavior this model has by ID
     */
    ModelBehavior* FindBehavior(uint32_t behaviorID);
	
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
};