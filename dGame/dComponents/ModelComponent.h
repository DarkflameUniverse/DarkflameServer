#pragma once
#include "dCommonVars.h"
#include "RakNetTypes.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "Component.h"

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
};