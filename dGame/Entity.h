#pragma once

#include <map>
#include <functional>
#include <typeinfo>
#include <type_traits>
#include <vector>

#include "../thirdparty/raknet/Source/Replica.h"
#include "../thirdparty/raknet/Source/ReplicaManager.h"

#include "dCommonVars.h"
#include "User.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "LDFFormat.h"
#include "Loot.h"
#include "Zone.h"

#include "EntityTimer.h"
#include "EntityCallbackTimer.h"
#include "EntityInfo.h"

class Player;
class Spawner;
class ScriptComponent;
class dpEntity;
class Component;
class Character;

/**
 * An entity in the world. Has multiple components.
 */
class Entity {
public:
    explicit Entity(const LWOOBJID& objectID, EntityInfo info, Entity* parentEntity = nullptr);
    virtual ~Entity();

	virtual void Initialize();
	
    bool operator==(const Entity& other) const;
    bool operator!=(const Entity& other) const;

	/**
	 * Getters
	 */
    
    const LWOOBJID& GetObjectID() const { return m_ObjectID; }

    const LOT GetLOT() const { return m_TemplateID; }

    Character* GetCharacter() const { return m_Character; }

    uint8_t GetGMLevel() const { return m_GMLevel; }

    uint8_t GetCollectibleID() const { return uint8_t(m_CollectibleID); }

	Entity* GetParentEntity() const { return m_ParentEntity; }

	LUTriggers::Trigger* GetTrigger() const { return m_Trigger; }

	std::vector<std::string>& GetGroups() { return m_Groups; };

	Spawner* GetSpawner() const { return m_Spawner; }
	
	LWOOBJID GetSpawnerID() const { return m_SpawnerID; }

	const std::vector<LDFBaseData*>& GetSettings() const { return m_Settings; }

	const std::vector<LDFBaseData*>& GetNetworkSettings() const { return m_NetworkSettings; }

	bool GetIsDead() const;

	bool GetPlayerReadyForUpdates() const { return m_PlayerIsReadyForUpdates;}

	bool GetIsGhostingCandidate() const;

	int8_t GetObservers() const;

	uint16_t GetNetworkId() const;

	Entity* GetOwner() const;

	const NiPoint3& GetDefaultPosition() const;

	const NiQuaternion& GetDefaultRotation() const;

	float GetDefaultScale() const;
	
	const NiPoint3& GetPosition() const;

	const NiQuaternion& GetRotation() const;

	virtual User* GetParentUser() const;

	virtual SystemAddress GetSystemAddress() const { return UNASSIGNED_SYSTEM_ADDRESS; };

	/**
	 * Setters
	 */

    void SetCharacter(Character* value) { m_Character = value; }
	
    void SetGMLevel(uint8_t value);

	void SetOwnerOverride(LWOOBJID value);

	void SetPlayerReadyForUpdates() { m_PlayerIsReadyForUpdates = true; }

	void SetObservers(int8_t value);

	void SetNetworkId(uint16_t id);

	void SetPosition(NiPoint3 position);

	void SetRotation(NiQuaternion rotation);

	virtual void SetRespawnPos(NiPoint3 position) {}

	virtual void SetRespawnRot(NiQuaternion rotation) {}
	
	virtual void SetSystemAddress(const SystemAddress& value) {};

	/**
	 * Component management
	 */

    Component* GetComponent(int32_t componentID) const;

	template<typename T>
	T* GetComponent() const;

	template<typename T>
	bool TryGetComponent(int32_t componentId, T*& component) const;

	bool HasComponent(int32_t componentId) const;

	void AddComponent(int32_t componentId, Component* component);

	std::vector<ScriptComponent*> GetScriptComponents();

	void SetProximityRadius(float proxRadius, std::string name);
	void SetProximityRadius(dpEntity* entity, std::string name);

	void AddChild(Entity* child);
	void RemoveChild(Entity* child);
	void RemoveParent();
	void AddTimer(std::string name, float time);
	void AddCallbackTimer(float time, std::function<void()> callback);
	bool HasTimer(const std::string& name);
	void CancelCallbackTimers();
	void CancelAllTimers();
	void CancelTimer(const std::string& name);

    void AddToGroup(const std::string& group);
	bool IsPlayer() const;

	std::unordered_map<int32_t, Component*>& GetComponents() { return m_Components; } // TODO: Remove
	
	void WriteBaseReplicaData(RakNet::BitStream* outBitStream, eReplicaPacketType packetType);
	void WriteComponents(RakNet::BitStream* outBitStream, eReplicaPacketType packetType);
    void ResetFlags();
    void UpdateXMLDoc(tinyxml2::XMLDocument* doc);
	void Update(float deltaTime);

	// Events
	void OnCollisionProximity(LWOOBJID otherEntity, const std::string& proxName, const std::string& status);
	void OnCollisionPhantom(LWOOBJID otherEntity);
    void OnCollisionLeavePhantom(LWOOBJID otherEntity);

    void OnFireEventServerSide(Entity* sender, std::string args, int32_t param1 = -1, int32_t param2 = -1, int32_t param3 = -1);
    void OnActivityStateChangeRequest(const LWOOBJID senderID, const int32_t value1, const int32_t value2,
                                      const std::u16string& stringValue);
    void OnCinematicUpdate(Entity* self, Entity* sender, eCinematicEvent event, const std::u16string& pathName,
                           float_t pathTime, float_t totalTime, int32_t waypoint);

	void NotifyObject(Entity* sender, const std::string& name, int32_t param1 = 0, int32_t param2 = 0);
	void OnEmoteReceived(int32_t emote, Entity* target);
    
    void OnUse(Entity* originator);

	void OnHitOrHealResult(Entity* attacker, int32_t damage);
	void OnHit(Entity* attacker);

	void OnZonePropertyEditBegin();
	void OnZonePropertyEditEnd();
	void OnZonePropertyModelEquipped();
	void OnZonePropertyModelPlaced(Entity* player);
	void OnZonePropertyModelPickedUp(Entity* player);
	void OnZonePropertyModelRemoved(Entity* player);
	void OnZonePropertyModelRemovedWhileEquipped(Entity* player);
	void OnZonePropertyModelRotated(Entity* player);

	void OnMessageBoxResponse(Entity* sender, int32_t button, const std::u16string& identifier, const std::u16string& userData);
	void OnChoiceBoxResponse(Entity* sender, int32_t button, const std::u16string& buttonIdentifier, const std::u16string& identifier);

	void Smash(const LWOOBJID source = LWOOBJID_EMPTY, const eKillType killType = eKillType::VIOLENT, const std::u16string& deathType = u"");
	void Kill(Entity* murderer = nullptr);
    void AddRebuildCompleteCallback(const std::function<void(Entity* user)>& callback) const;
	void AddCollisionPhantomCallback(const std::function<void(Entity* target)>& callback);
	void AddDieCallback(const std::function<void()>& callback);
	void Resurrect();

	void AddLootItem(const Loot::Info& info);
	void PickupItem(const LWOOBJID& objectID);

	bool CanPickupCoins(uint64_t count);
	void RegisterCoinDrop(uint64_t count);

	void ScheduleKillAfterUpdate(Entity* murderer = nullptr);
	void TriggerEvent(std::string eveneventtID, Entity* optionalTarget = nullptr);
	void ScheduleDestructionAfterUpdate() { m_ShouldDestroyAfterUpdate = true; }
	void HandleTriggerCommand(std::string id, std::string target, std::string targetName, std::string args, Entity* optionalTarget);

	virtual NiPoint3 GetRespawnPosition() const { return NiPoint3::ZERO; }
	virtual NiQuaternion GetRespawnRotation() const { return NiQuaternion::IDENTITY; }

	void Sleep();
	void Wake();
	bool IsSleeping() const;

	/*
	 * Utility
	 */
	/**
	 * Retroactively corrects the model vault size due to incorrect initialization in a previous patch.
	 * 
	 */
	void RetroactiveVaultSize();
	bool GetBoolean(const std::u16string& name) const;
	int32_t GetI32(const std::u16string& name) const;
	int64_t GetI64(const std::u16string& name) const;
	
	void SetBoolean(const std::u16string& name, bool value);
	void SetI32(const std::u16string& name, int32_t value);
	void SetI64(const std::u16string& name, int64_t value);

	bool HasVar(const std::u16string& name) const;

	template<typename T>
	const T& GetVar(const std::u16string& name) const;

	template<typename T>
	void SetVar(const std::u16string& name, T value);

	void SendNetworkVar(const std::string& data, const SystemAddress& sysAddr);

	template<typename T>
	void SetNetworkVar(const std::u16string& name, T value, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);

    template<typename T>
    void SetNetworkVar(const std::u16string& name, std::vector<T> value, const SystemAddress& sysAddr = UNASSIGNED_SYSTEM_ADDRESS);

    template<typename T>
    T GetNetworkVar(const std::u16string& name);

	/**
	 * Get the LDF value and cast it as T.
	 */
	template<typename T>
	T GetVarAs(const std::u16string& name) const;

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
    
	LUTriggers::Trigger* m_Trigger;

	Character* m_Character;
	
    Entity* m_ParentEntity; //For spawners and the like
	std::vector<Entity*> m_ChildEntities;
    uint8_t m_GMLevel;
    uint16_t m_CollectibleID;
	std::vector<std::string> m_Groups;
	uint16_t m_NetworkID;
	std::vector<std::function<void()>> m_DieCallbacks;
    std::vector<std::function<void(Entity* target)>> m_PhantomCollisionCallbacks;
    
    std::unordered_map<int32_t, Component*> m_Components; //The int is the ID of the component
	std::vector<EntityTimer*> m_Timers;
	std::vector<EntityTimer*> m_PendingTimers;
	std::vector<EntityCallbackTimer*> m_CallbackTimers;

	bool m_ShouldDestroyAfterUpdate = false;

	LWOOBJID m_OwnerOverride;

	Entity* m_ScheduleKiller;

	bool m_PlayerIsReadyForUpdates = false;

	bool m_IsGhostingCandidate = false;

	int8_t m_Observers = 0;

	bool m_IsParentChildDirty = true;

	/*
	 * Collision
	 */
	std::vector<LWOOBJID> m_TargetsInPhantom;
};

/**
 * Template definitions.
 */

template<typename T>
bool Entity::TryGetComponent(const int32_t componentId, T*& component) const
{
	const auto& index = m_Components.find(componentId);

	if (index == m_Components.end())
	{
		component = nullptr;

		return false;
	}

	component = dynamic_cast<T*>(index->second);

	return true;
}

template <typename T>
T* Entity::GetComponent() const
{
	return dynamic_cast<T*>(GetComponent(T::ComponentType));
}


template<typename T>
const T& Entity::GetVar(const std::u16string& name) const
{
	auto* data = GetVarData(name);

	if (data == nullptr)
	{
		return LDFData<T>::Default;
	}

	auto* typed = dynamic_cast<LDFData<T>*>(data);

	if (typed == nullptr)
	{
		return LDFData<T>::Default;
	}

	return typed->GetValue();
}

template<typename T>
T Entity::GetVarAs(const std::u16string& name) const
{
	const auto data = GetVarAsString(name);

	T value;

	if (!GeneralUtils::TryParse(data, value))
	{
		return LDFData<T>::Default;
	}

	return value;
}

template<typename T>
void Entity::SetVar(const std::u16string& name, T value)
{
	auto* data = GetVarData(name);

	if (data == nullptr)
	{
		auto* data = new LDFData<T>(name, value);

		m_Settings.push_back(data);

		return;
	}

	auto* typed = dynamic_cast<LDFData<T>*>(data);

	if (typed == nullptr)
	{
		return;
	}

	typed->SetValue(value);
}

template<typename T>
void Entity::SetNetworkVar(const std::u16string& name, T value, const SystemAddress& sysAddr) {
    LDFData<T>* newData = nullptr;

    for (auto* data :m_NetworkSettings) {
        if (data->GetKey() != name)
            continue;

        newData = dynamic_cast<LDFData<T>*>(data);
        if (newData != nullptr) {
            newData->SetValue(value);
        } else {  // If we're changing types
            m_NetworkSettings.erase(
				std::remove(m_NetworkSettings.begin(), m_NetworkSettings.end(), data), m_NetworkSettings.end()
			);
            delete data;
        }

        break;
    }

    if (newData == nullptr) {
        newData = new LDFData<T>(name, value);
    }

    m_NetworkSettings.push_back(newData);
    SendNetworkVar(newData->GetString(true), sysAddr);
}

template<typename T>
void Entity::SetNetworkVar(const std::u16string& name, std::vector<T> values, const SystemAddress& sysAddr) {
    std::stringstream updates;
    auto index = 1;

    for (const auto& value : values) {
        LDFData<T>* newData = nullptr;
        const auto& indexedName = name + u"." + GeneralUtils::to_u16string(index);

        for (auto* data : m_NetworkSettings) {
            if (data->GetKey() != indexedName)
                continue;

            newData = dynamic_cast<LDFData<T>*>(data);
            newData->SetValue(value);
            break;
        }

        if (newData == nullptr) {
            newData = new LDFData<T>(indexedName, value);
        }

        m_NetworkSettings.push_back(newData);

        if (index == values.size()) {
            updates << newData->GetString(true);
        } else {
            updates << newData->GetString(true) << "\n";
        }

        index++;
    }

    SendNetworkVar(updates.str(), sysAddr);
}

template<typename T>
T Entity::GetNetworkVar(const std::u16string& name) {
    for (auto* data : m_NetworkSettings) {
        if (data == nullptr || data->GetKey() != name)
            continue;

        auto* typed = dynamic_cast<LDFData<T>*>(data);
        if (typed == nullptr)
            continue;

        return typed->GetValue();
    }

    return LDFData<T>::Default;
}
