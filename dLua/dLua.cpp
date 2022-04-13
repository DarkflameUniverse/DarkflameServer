#include "dLua.h"

#include "lEntity.h"
#include "EntityManager.h"
#include "LuaScript.h"
#include "LuaTerminateBehavior.h"
#include "ScriptComponent.h"
#include "Game.h"
#include "dLogger.h"

namespace dLua
{
    void LoadScript(Entity* entity, const std::string& name)
    {
        // Read the script from the file.
        std::stringstream input;

        std::ifstream file(name);

        if (!file.is_open())
            throw std::runtime_error("LoadScript: Failed to open file");

        std::string line;
        while (std::getline(file, line)) {
            input << line << "\n";
        }

		LuaScript* lua = new LuaScript(LuaTerminateBehavior::ResetScriptAndDelete);

		ScriptComponent* script = entity->GetComponent<ScriptComponent>();

		if (script == nullptr) {
			script = new ScriptComponent(entity, false);
			
			entity->AddComponent(ScriptComponent::ComponentType, script);
		}

		script->SetScript(lua);

		lua->SetEntity(entity);

		// Run the code, and print any errors
		try {
			lua->Script(input.str());
		}
		// Catch sol::error
		catch (const sol::error& e) {
			Game::logger->Log("dLua", std::string(e.what()));
		}
		// Catch *any* error
		catch (...) {
			Game::logger->Log("dLua", "Unknown error loading script!");
		}
    }

    Entity* GetEntity(const sol::state& lua, const std::string& key)
    {
        sol::object object = lua[key];

        if (!object.is<lEntity>())
            return nullptr;

        return object.as<lEntity>().GetEntity();
    }
    
    Entity* GetEntityOrThrow(const sol::state& lua, const std::string& key)
    {
        sol::object object = lua[key];

        if (!object.is<lEntity>())
            throw std::runtime_error("GetEntityOrThrow: Invalid key");

        lEntity entity = object.as<lEntity>();
        
        if (!entity.IsValid())
            throw std::runtime_error("GetEntityOrThrow: Invalid entity");

        return entity.GetEntity();
    }

    Entity* GetEntity(const LWOOBJID& id)
    {
        return EntityManager::Instance()->GetEntity(id);
    }

    Entity* GetEntityOrThrow(const LWOOBJID& id)
    {
        Entity* entity = EntityManager::Instance()->GetEntity(id);

        if (!entity)
        {
            throw sol::error("Entity not found.");
        }

        return entity;
    }    

    Entity* GetEntity(sol::object object)
    {
        LWOOBJID id = LWOOBJID_EMPTY;

        if (object.is<lEntity>()) {
            id = object.as<lEntity>().GetID();
        } else if (object.is<LWOOBJID>()) {
            id = object.as<LWOOBJID>();
        } else if (object.is<std::string>()) {
            id = atol(object.as<std::string>().c_str());
        } else {
            return nullptr;
        }

        return GetEntity(id);
    }

    Entity* GetEntityOrThrow(sol::object object)
    {
        Entity* entity = GetEntity(object);

        if (!entity)
        {
            throw sol::error("Entity not found.");
        }

        return entity;
    }
    
    LWOOBJID GetEntityID(sol::object object)
    {
        LWOOBJID id = LWOOBJID_EMPTY;

        if (object.is<lEntity>()) {
            id = object.as<lEntity>().GetID();
        } else if (object.is<LWOOBJID>()) {
            id = object.as<LWOOBJID>();
        } else if (object.is<std::string>()) {
            id = atol(object.as<std::string>().c_str());
        } else {
            return LWOOBJID_EMPTY;
        }
        
        return id;
    }

    LWOOBJID GetEntityIDOrThrow(sol::object object)
    {
        LWOOBJID id = GetEntityID(object);

        if (id == LWOOBJID_EMPTY)
        {
            throw sol::error("Entity not found.");
        }

        return id;
    }
}
