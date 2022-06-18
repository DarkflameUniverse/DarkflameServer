#pragma once

#include "../thirdparty/tinyxml2/tinyxml2.h"
#include "GameMessage.h"

class Entity;

/**
 * Component base class, provides methods for game loop updates, usage events and loading and saving to XML.
 */
class Component
{
public:
    Component(Entity* parent);
    virtual ~Component();

    /**
     * Gets the owner of this component
     * @return the owner of this component
     */
    Entity* GetParent() const;

    /**
     * Updates the component in the game loop
     * @param deltaTime time passed since last update
     */
    virtual void Update(float deltaTime);

    /**
     * Event called when this component is being used, e.g. when some entity interacted with it
     * @param originator
     */
    virtual void OnUse(Entity* originator); 

    /**
     * Save data from this componennt to character XML
     * @param doc the document to write data to
     */
    virtual void UpdateXml(tinyxml2::XMLDocument* doc);

    /**
     * Load base data for this component from character XML
     * @param doc the document to read data from
     */
    virtual void LoadFromXml(tinyxml2::XMLDocument* doc);

    using Handler = void(Component::*)(GameMessage*);
    inline void RegisterGM(uint32_t id, Handler handler) { m_Handlers.insert({ id, handler }); }

    std::unordered_map<uint32_t, Handler> GetHandlers() { return m_Handlers; }

protected:

    /**
     * The entity that owns this component
     */
    Entity* m_Parent;

    std::unordered_map<uint32_t, Handler> m_Handlers = std::unordered_map<uint32_t, Handler>();
};
