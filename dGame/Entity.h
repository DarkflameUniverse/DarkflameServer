#ifndef __ENTITY__H__
#define __ENTITY__H__

#include <map>
#include <functional>
#include <typeinfo>
#include <type_traits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"
#include "eKillType.h"

namespace Loot {
	class Info;
};

namespace tinyxml2 {
	class XMLDocument;
};

class Player;
class EntityInfo;
class User;
class Spawner;
class ScriptComponent;
class dpEntity;
class EntityTimer;
class Component;
class Item;
class Character;
class EntityCallbackTimer;
enum class eTriggerEventType;
enum class eGameMasterLevel : uint8_t;
enum class eReplicaComponentType : uint32_t;
enum class eReplicaPacketType : uint8_t;
enum class eCinematicEvent : uint32_t;

namespace CppScripts {
	class Script;
};

/**
 * An entity in the world.
 * Entities are composed of components which define their behavior.
 */

using ComponentPtr = std::unique_ptr<Component>;
using ComponentWhitelist = std::vector<eReplicaComponentType>;
using TemplateComponents = std::vector<std::pair<eReplicaComponentType, uint32_t>>;

class Entity {
public:
	explicit Entity(const LWOOBJID& objectID, EntityInfo info, Entity* parentEntity = nullptr);
	virtual ~Entity();

	void ApplyComponentWhitelist(TemplateComponents& components) const;
	static const std::vector<ComponentWhitelist>& GetComponentWhitelists() { return m_ComponentWhitelists; }

	// There are some very very edge cases we need to take care of with what components
	// are kept and removed. Here is where we take care of those cases.
	void ApplyComponentBlacklist(TemplateComponents& components) const;

	// For adding and removing components based on LDF keys
	void ApplyComponentConfig(TemplateComponents& components) const;

	// Paths have several components they could add. This function will add them.
	void AddPathComponent(TemplateComponents& components) const;

	void IsGhosted();
	virtual void Initialize();

	bool operator==(const Entity& other) const;
	bool operator!=(const Entity& other) const;

	/**
	 * Getters
	 */
	Character* GetCharacter() const { return m_Character; }
	void SetCharacter(Character* value) { m_Character = value; }

	const LWOOBJID GetObjectID() const { return m_ObjectID; }

	const LOT GetLOT() const { return m_TemplateID; }

	eGameMasterLevel GetGMLevel() const { return m_GMLevel; }
	void SetGMLevel(const eGameMasterLevel value);

	Entity* GetParentEntity() const { return m_ParentEntity; }

	const std::vector<std::string>& GetGroups() { return m_Groups; };
	void SetGroups(const std::vector<std::string>& value) { m_Groups = value; }

	Spawner* GetSpawner() const { return m_Spawner; }

	LWOOBJID GetSpawnerID() const { return m_SpawnerID; }

	const std::vector<LDFBaseData*>& GetSettings() const { return m_Settings; }

	const std::vector<LDFBaseData*>& GetNetworkSettings() const { return m_NetworkSettings; }

	bool GetIsDead() const;

	bool GetPlayerReadyForUpdates() const { return m_PlayerIsReadyForUpdates; }
	void SetPlayerReadyForUpdates() { m_PlayerIsReadyForUpdates = true; }

	const bool GetIsGhostingCandidate() const { return m_IsGhostingCandidate; }

	const int8_t GetObservers() const { return m_Observers; }
	void SetObservers(const int8_t value);

	const uint16_t GetNetworkId() const { return m_NetworkID; }
	void SetNetworkId(const uint16_t id);

	Entity* GetOwner() const;
	void SetOwnerOverride(const LWOOBJID& value);

	const NiPoint3& GetDefaultPosition() const;

	const NiQuaternion& GetDefaultRotation() const;

	const float GetDefaultScale() const { return m_Scale; }

	const NiPoint3& GetPosition() const;
	void SetPosition(const NiPoint3& position);

	const NiQuaternion& GetRotation() const;
	void SetRotation(const NiQuaternion& rotation);

	virtual User* GetParentUser() const;

	virtual const SystemAddress GetSystemAddress() const { return UNASSIGNED_SYSTEM_ADDRESS; };

	virtual void SetRespawnPos(const NiPoint3& position) {};

	virtual void SetRespawnRot(const NiQuaternion& rotation) {};

	virtual void SetSystemAddress(const SystemAddress& value) {};

	/**
	 * Component management
	 */
	bool HasComponent(const eReplicaComponentType componentId) const;

	std::vector<ScriptComponent*> GetScriptComponents();

	void Subscribe(const LWOOBJID& scriptObjId, CppScripts::Script* scriptToAdd, const std::string& notificationName);
	void Unsubscribe(const LWOOBJID& scriptObjId, const std::string& notificationName);

	void SetProximityRadius(const float proxRadius, const std::string& name);
	void SetProximityRadius(dpEntity* entity, const std::string& name);

	void AddChild(Entity* child);
	void RemoveChild(Entity* child);
	void RemoveParent();
	void AddTimer(const std::string& name, const float time);
	void AddCallbackTimer(const float time, const std::function<void()>& callback);
	bool HasTimer(const std::string& name);
	void CancelCallbackTimers();
	void CancelAllTimers();
	void CancelTimer(const std::string& name);

	void AddToGroups(const std::string& group);
	bool IsPlayer() const;

	void WriteBaseReplicaData(RakNet::BitStream* outBitStream, const eReplicaPacketType packetType);
	void WriteComponents(RakNet::BitStream* outBitStream, const eReplicaPacketType packetType);
	void ResetFlags();
	void UpdateXMLDoc(tinyxml2::XMLDocument* doc);
	void Update(float deltaTime);

	// Events
	void OnCollisionProximity(const LWOOBJID otherEntity, const std::string& proxName, const std::string& status);
	void OnCollisionPhantom(const LWOOBJID otherEntity);
	void OnCollisionLeavePhantom(const LWOOBJID otherEntity);

	void OnFireEventServerSide(Entity* sender, const std::string args, const int32_t param1 = -1, const int32_t param2 = -1, const int32_t param3 = -1);
	void OnActivityStateChangeRequest(const LWOOBJID senderID, const int32_t value1, const int32_t value2,
		const std::u16string& stringValue);
	void OnCinematicUpdate(Entity* self, Entity* sender, const eCinematicEvent event, const std::u16string& pathName,
		const float pathTime, const float totalTime, const int32_t waypoint);

	void NotifyObject(Entity* sender, const std::string& name, const int32_t param1 = 0, const int32_t param2 = 0);
	void OnEmoteReceived(const int32_t emote, Entity* target);

	void OnUse(Entity* originator);

	void OnHitOrHealResult(Entity* attacker, const int32_t damage);
	void OnHit(Entity* attacker);

	void OnZonePropertyEditBegin();
	void OnZonePropertyEditEnd();
	void OnZonePropertyModelEquipped();
	void OnZonePropertyModelPlaced(Entity* player);
	void OnZonePropertyModelPickedUp(Entity* player);
	void OnZonePropertyModelRemoved(Entity* player);
	void OnZonePropertyModelRemovedWhileEquipped(Entity* player);
	void OnZonePropertyModelRotated(Entity* player);

	void OnMessageBoxResponse(Entity* sender, const int32_t button, const std::u16string& identifier, const std::u16string& userData);
	void OnChoiceBoxResponse(Entity* sender, const int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier);
	void RequestActivityExit(Entity* sender, const LWOOBJID& player, const bool canceled);

	void Smash(const LWOOBJID source = LWOOBJID_EMPTY, const eKillType killType = eKillType::VIOLENT, const std::u16string& deathType = u"");
	void Kill(Entity* murderer = nullptr);
	void AddRebuildCompleteCallback(const std::function<void(Entity* user)>& callback) const;
	void AddCollisionPhantomCallback(const std::function<void(Entity* target)>& callback);
	void AddDieCallback(const std::function<void()>& callback);
	void Resurrect();

	void AddLootItem(const Loot::Info& info);
	void PickupItem(const LWOOBJID& objectID);

	bool CanPickupCoins(const uint64_t& count);
	void RegisterCoinDrop(const uint64_t& count);

	void ScheduleKillAfterUpdate(Entity* murderer = nullptr);
	void TriggerEvent(const eTriggerEventType event, Entity* optionalTarget = nullptr);
	void ScheduleDestructionAfterUpdate() { m_ShouldDestroyAfterUpdate = true; }

	virtual NiPoint3 GetRespawnPosition() const { return NiPoint3::ZERO; }
	virtual NiQuaternion GetRespawnRotation() const { return NiQuaternion::IDENTITY; }
	CppScripts::Script* GetScript() const;

	void Sleep();
	void Wake();
	bool IsSleeping() const;

	/*
	 * Utility
	 */

	 //Retroactively corrects the model vault size due to incorrect initialization in a previous patch.
	void RetroactiveVaultSize();
	bool GetBoolean(const std::u16string& name) const;
	int32_t GetI32(const std::u16string& name) const;
	int64_t GetI64(const std::u16string& name) const;

	void SetBoolean(const std::u16string& name, bool value);
	void SetI32(const std::u16string& name, int32_t value);
	void SetI64(const std::u16string& name, int64_t value);

	bool HasVar(const std::u16string& name) const;

	/**
	 * Get the LDF data.
	 */
	LDFBaseData* GetVarData(const std::u16string& name) const;

	/**
	 * Get the LDF value and convert it to a string.
	 */
	std::string GetVarAsString(const std::u16string& name) const;

	/*
	 * Collision
	 */
	std::vector<LWOOBJID>& GetTargetsInPhantom();

	Entity* GetScheduledKiller() { return m_ScheduleKiller; }

	const std::unordered_map<eReplicaComponentType, ComponentPtr>& GetComponents() { return m_Components; }

	// Template declarations
	template<typename T>
	const T& GetVar(const std::u16string& name) const;

	template<typename T>
	T GetVarAs(const std::u16string& name) const;

	template<typename T>
	void SetVar(const std::u16string& name, const T& value);

	template<typename T>
	T GetNetworkVar(const std::u16string& name);

	void SendNetworkVar(const std::string& data, const SystemAddress& sysAddr);

	template<typename T>
	void SetNetworkVar(const std::u16string& name, const T& value, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);

	template<typename T>
	void SetNetworkVar(const std::u16string& name, const std::vector<T>& value, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);
	/**
	 * @brief Get a non-owning reference to a component
	 *
	 * @tparam Cmpt The component to get a non-owning reference of
	 * @return Cmpt* The non-owning pointer to the component
	 */
	template<typename Cmpt>
	Cmpt* GetComponent() const;

	/**
	 * @brief Adds a component to this Entity.
	 *
	 * @tparam Cmpt The component to create
	 * @tparam ConstructorValues The constructor values to forward to the component
	 * @param arguments The constructor values to forward to the component
	 * @return Cmpt* A non-owning pointer to the created component,
	 * or a non-owning pointer to the existing component if the component already existed.
	 */
	template<typename Cmpt, typename...ConstructorValues>
	Cmpt* AddComponent(ConstructorValues... arguments);

	/**
	 * @brief Removes a component from this Entity.
	 */
	template<typename Cmpt>
	void RemoveComponent();

protected:
	LWOOBJID m_ObjectID;

	LOT m_TemplateID;

	std::vector<LDFBaseData*> m_Settings;
	std::vector<LDFBaseData*> m_NetworkSettings;

	NiPoint3 m_DefaultPosition;
	NiQuaternion m_DefaultRotation;
	float m_Scale;

	Spawner* m_Spawner;
	LWOOBJID m_SpawnerID;

	bool m_HasSpawnerNodeID;
	uint32_t m_SpawnerNodeID;

	Character* m_Character;

	Entity* m_ParentEntity; //For spawners and the like
	std::vector<Entity*> m_ChildEntities;
	eGameMasterLevel m_GMLevel;
	std::vector<std::string> m_Groups;
	uint16_t m_NetworkID;
	std::vector<std::function<void()>> m_DieCallbacks;
	std::vector<std::function<void(Entity*)>> m_PhantomCollisionCallbacks;

	std::unordered_map<eReplicaComponentType, ComponentPtr> m_Components;
	std::vector<EntityTimer*> m_Timers;
	std::vector<EntityTimer*> m_PendingTimers;
	std::vector<EntityCallbackTimer*> m_CallbackTimers;

	bool m_ShouldDestroyAfterUpdate;

	LWOOBJID m_OwnerOverride;

	Entity* m_ScheduleKiller;

	bool m_PlayerIsReadyForUpdates;

	bool m_IsGhostingCandidate;

	int8_t m_Observers;

	bool m_IsParentChildDirty;

	/*
	 * Collision
	 */
	std::vector<LWOOBJID> m_TargetsInPhantom;

	static const std::vector<ComponentWhitelist> m_ComponentWhitelists;
};

#include "Entity.tcc"

#endif  //!__ENTITY__H__
