#pragma once

namespace tinyxml2 {
	class XMLDocument;
}

namespace RakNet {
	class BitStream;
}

namespace GameMessages {
	struct GameMsg;
	struct RequestUseEvent;
}

class Entity;

/**
 * Component base class, with the client's component lifecycle (LWOGameObjComponent's vtable,
 * LWOGameObjComponentFunctions), which the entity calls in the client's order (ObjectLoader2::DoObjectComponentLoad):
 * constructed, LoadDataFromTemplate (its CDClient row), PrepareConfigData and LoadConfigData (the object's level
 * config), LoadFromSaveData (a character's save), then Startup once every component of the object exists; Run every
 * frame; OnEnterScope / OnLeaveScope when a player starts or stops seeing the object; Preshutdown and Shutdown before it
 * is deleted. Serialize is the server side of the client's Deserialize. Client-only slots (GetSizeInMemory,
 * GraphicsDeviceReset, GetPickTypePriority) are left out.
 */
class Component {
public:
	Component(Entity* parent, const int32_t componentID) : m_Parent{ parent }, m_ComponentID{componentID} {}
	virtual ~Component() = default;

	/**
	 * Gets the owner of this component
	 * @return the owner of this component
	 */
	Entity* GetParent() const { return m_Parent; }

	[[nodiscard]] int32_t GetComponentID() const noexcept { return m_ComponentID; }

	// The CDClient row (componentID) of the component's table
	virtual void LoadDataFromTemplate() {}

	// The object's level config (LDF): PrepareConfigData first, then LoadConfigData
	virtual void PrepareConfigData() {}
	virtual void LoadConfigData() {}

	// After every component of the object exists
	virtual void Startup() {}

	// Every frame
	virtual void Run(float deltaTime) {}

	// A player starts or stops seeing the object (ghosting)
	virtual void OnEnterScope(Entity& player) {}
	virtual void OnLeaveScope(Entity& player) {}

	// Before the object is deleted
	virtual void Preshutdown() {}
	virtual void Shutdown() {}

	/**
	 * Event called when this component is being used, e.g. when some entity interacted with it
	 * @param originator
	 */

	// A character's save (its XML): read on load, written on save
	virtual void LoadFromSaveData(const tinyxml2::XMLDocument& doc) {}
	virtual void WriteSaveData(tinyxml2::XMLDocument& doc) {}

	virtual void Serialize(RakNet::BitStream& outBitStream, bool isConstruction) {}
protected:
	template<typename GameObjClass, typename DerivedMsg>
	inline void RegisterMsg(bool (GameObjClass::*handler)(DerivedMsg&)) {
		static_assert(std::is_base_of_v<GameMessages::GameMsg, DerivedMsg>, "DerivedMsg must inherit from GameMsg");
		static_assert(std::is_base_of_v<Component, GameObjClass>, "GameObjClass must inherit from Component");
		const auto handlerBound = std::bind(handler, static_cast<GameObjClass*>(this), std::placeholders::_1);
		const auto castWrapper = [handlerBound](GameMessages::GameMsg& msg) {
			return handlerBound(static_cast<DerivedMsg&>(msg));
		};

		DerivedMsg msg;
		m_Parent->RegisterMsg(msg.msgId, castWrapper);
	}

	/**
	 * The entity that owns this component
	 */
	Entity* m_Parent;

	// The component ID, this should never be changed after initialization
	// This is used in various different ways
	// 1. To identify which entry this component is in its corresponding table
	// 2. To mark that an Entity should have the component with no database entry (it will be 0 in this case)
	// 3. The component exists implicitly due to design (CollectibleComponent always has a DestructibleComponent accompanying it). In this case the ID will be -1.
	const int32_t m_ComponentID;
};
