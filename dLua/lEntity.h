#pragma once

#include "dLua.h"

#include "Entity.h"

class LuaScript;

/**
 * A struct for wrapping an Entity in Lua.
 */
struct lEntity {
    static void RegisterClass(LuaScript* script);

    LWOOBJID id;

    lEntity() : id(LWOOBJID_EMPTY) {}

    lEntity(LWOOBJID id) : id(id) {}

    lEntity(Entity* entity);
    
    /**
     * Checks if this entity is valid.
     */
    bool IsValid() const;

    /**
     * Get the entity's ID.
     */
    LWOOBJID GetID() const;

    /**
     * Get the entity's lot.
     */
    LOT GetLOT() const;

    /**
     * Get the entity's position.
     */
    NiPoint3 GetPosition() const;

    /**
     * Get the entity's rotation.
     */
    NiQuaternion GetRotation() const;

    /**
     * Set the entit's position.
     */
    void SetPosition(const NiPoint3& pos);

    /**
     * Set the entity's rotation.
     */
    void SetRotation(const NiQuaternion& rot);
    
    /**
     * Set a variable in the entity's data table.
     */
    void SetVar(const std::string& key, sol::object value);

    /**
     * Get a variable from the entity's data table.
     */
    sol::object GetVar(LuaScript* script, const std::string& key);

    /**
     * Smash this entity.
     */
    void Smash(sol::object smasher);

    /**
     * Add a timer.
     */
    void AddTimer(const std::string& name, float seconds);

    /**
     * Load a LUA script and attach it to this entity.
     */
    void LoadScript(const std::string& name);

    /**
     * Serialize this entity.
     */
    void Serialize();

    Entity* GetEntity() const;

    Entity* GetEntityOrThrow() const;

    Entity* operator->() const;

    /**
     * Assets that this entity has a specific component.
     */
    template <typename T>
    void AssertComponent() {
        if (!GetEntity()->HasComponent(T::ComponentType)) {
            throw std::runtime_error("Entity does not have component " + std::to_string((int32_t) T::ComponentType));
        }
    }
    
    /**
     * Assets that an entity has a specific component.
     */
    template <typename T>
    static inline void AssertComponent(lEntity entity) {
        if (!entity->HasComponent(T::ComponentType)) {
            throw std::runtime_error("Entity does not have component " + std::to_string((int32_t) T::ComponentType));
        }
    }

    /**
     * Utility for getting a component, asserting it exists
     */
    template <typename T>
    static inline T* GetComponent(lEntity entity) {
        entity.AssertComponent<T>();
        return entity->GetComponent<T>();
    }
};
