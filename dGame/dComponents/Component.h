#pragma once

namespace tinyxml2 {
	class XMLDocument;
}

namespace RakNet {
	class BitStream;
}

namespace GameMessages {
	struct GameMsg;
}

class Entity;

/**
 * Component base class, provides methods for game loop updates, usage events and loading and saving to XML.
 */
class Component {
public:
	Component(Entity* parent) : m_Parent{ parent } {}
	virtual ~Component() = default;

	/**
	 * Gets the owner of this component
	 * @return the owner of this component
	 */
	Entity* GetParent() const { return m_Parent; }

	/**
	 * Updates the component in the game loop
	 * @param deltaTime time passed since last update
	 */
	virtual void Update(float deltaTime) {}

	/**
	 * Event called when this component is being used, e.g. when some entity interacted with it
	 * @param originator
	 */
	virtual void OnUse(Entity* originator) {}

	/**
	 * Save data from this componennt to character XML
	 * @param doc the document to write data to
	 */
	virtual void UpdateXml(tinyxml2::XMLDocument& doc) {}

	/**
	 * Load base data for this component from character XML
	 * @param doc the document to read data from
	 */
	virtual void LoadFromXml(const tinyxml2::XMLDocument& doc) {}

	virtual void Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {}

protected:

	void RegisterMsg(const MessageType::Game msgId, auto* self, const auto handler) {
		m_Parent->RegisterMsg(msgId, std::bind(handler, self, std::placeholders::_1));
	}

	/**
	 * The entity that owns this component
	 */
	Entity* m_Parent;
};
