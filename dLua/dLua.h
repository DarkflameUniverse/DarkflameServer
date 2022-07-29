#pragma once

#include "sol/sol.hpp"
#include "dCommonVars.h"

class Entity;

namespace dLua
{
    void LoadScript(Entity* entity, const std::string& name);

    Entity* GetEntity(const sol::state& lua, const std::string& key);

    Entity* GetEntityOrThrow(const sol::state& lua, const std::string& key);

    Entity* GetEntity(const LWOOBJID& id);

    Entity* GetEntityOrThrow(const LWOOBJID& id);

    Entity* GetEntity(sol::object object);

    Entity* GetEntityOrThrow(sol::object object);

    LWOOBJID GetEntityID(sol::object object);

    LWOOBJID GetEntityIDOrThrow(sol::object object);

    template<typename T>
    T TableGet(const sol::table& table, const std::string& key)
    {
        sol::object object = table[key];
        if (!object.valid())
            throw std::runtime_error("TableGet: Missing key");
        if (!object.is<T>())
            throw std::runtime_error("TableGet: Invalid key");
        return object.as<T>();
    }

    template<typename T>
    T TableGet(const sol::table& table, const std::string& key, const T& defaultValue)
    {
        sol::object object = table[key];
        if (!object.valid())
            return defaultValue;
        if (!object.is<T>())
            throw std::runtime_error("TableGet: Invalid key");
        return object.as<T>();
    }

    template<typename T>
    bool TryTableGet(const sol::table& table, const std::string& key, T& out)
    {
        sol::object object = table[key];
        if (!object.valid())
            return false;
        if (!object.is<T>())
            throw std::runtime_error("TableGet: Invalid key");
        out = object.as<T>();
        return true;
    }
}