#pragma once

#include "tinyxml2.h"

class Entity;

/**
 * Component base class, provides methods for game loop updates, usage events and loading and saving to XML.
 */
class Component {
public:
	Component(Entity* owningEntity);
	virtual ~Component();

	/**
	 * Gets the owner of this component
	 * @return the owner of this component
	 */
	Entity* GetOwningEntity() const { return m_OwningEntity; };

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

	/**
	 * Call after you have newed the component to initialize it
	 */
	virtual void Startup();

	/**
	 * Updates the component in the game loop
	 * @param deltaTime time passed since last update
	 */
	virtual void Update(float deltaTime);

	/**
	 * Loads the data of this component from the luz/lvl configuration
	 */
	virtual void LoadConfigData();

	/**
	 * Loads the data of this component from the cdclient database
	 */
	virtual void LoadTemplateData();
protected:

	/**
	 * The entity that owns this component
	 */
	Entity* m_OwningEntity;
};
