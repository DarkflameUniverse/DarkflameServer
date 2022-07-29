#include "LuaScript.h"

#include "Entity.h"
#include "lCommonTypes.h"
#include "lEntity.h"
#include "ChatPackets.h"
#include "CppScripts.h"
#include "ScriptComponent.h"
#include "Game.h"
#include "dLogger.h"

LuaScript::LuaScript(LuaTerminateBehavior terminateBehavior)
{
    m_TerminateBehavior = terminateBehavior;
    m_Hooks = 0;
    m_Terminating = false;

    m_State = sol::state();

    m_State.open_libraries(sol::lib::base, sol::lib::package);

    lCommonTypes::RegisterClass(this);
    lEntity::RegisterClass(this);
    
    // Register a function to print to log
    m_State.set_function("print", [](const std::string& message) {
        Game::logger->Log("dLua", "%s\n", message.c_str());
    });

    // Register a function to terminate the script
    m_State.set_function("terminate", [this]() {
        m_Terminating = true;

        auto* scriptComponent = GetEntity()->GetComponent<ScriptComponent>();

        if (scriptComponent == nullptr || scriptComponent->GetScript() != this)
        {
            return;
        }
        
        switch (m_TerminateBehavior)
        {
            case LuaTerminateBehavior::None:
                break;
            case LuaTerminateBehavior::ResetScript:
            case LuaTerminateBehavior::ResetScriptAndDelete:
                scriptComponent->SetScript(CppScripts::GetInvalidScript());
                break;
        }
    });

    // Register a function to add a callback timer
    m_State.set_function("addCallbackTimer", [this](float seconds, sol::function callback) {
        AddCallbackTimer(seconds, callback);
    });
}

sol::state& LuaScript::GetState()
{
    return m_State;
}

void LuaScript::Script(const std::string& script)
{
    m_State.script(script);
}

void LuaScript::SetEntity(Entity* entity)
{
    m_State["self"] = lEntity(entity->GetObjectID());
}

Entity* LuaScript::GetEntity()
{
    lEntity entity = m_State["self"];

    return dLua::GetEntityOrThrow(entity.id);
}

void LuaScript::AddCallbackTimer(float seconds, sol::function callback) {
    if (m_Terminating) {
        Game::logger->Log("LuaScript", "Script is terminating, ignoring addCallbackTimer\n");
        return;
    }

    m_Hooks++;

    GetEntity()->AddCallbackTimer(seconds, [this, callback]() {
        m_Hooks--;

        CheckHooks();

        if (m_Terminating) {
            Game::logger->Log("LuaScript", "Script is terminating, ignoring callback\n");
            return;
        }
        
        try {
            callback();
        }
        catch (sol::error& e) {
            Game::logger->Log("LuaScript", "Error in callback: %s\n", e.what());
        }
    });
}

void LuaScript::CheckHooks()
{
    if (m_Hooks != 0 || m_AttemptingToFinalize || !m_Terminating) {
        return;
    }

    m_AttemptingToFinalize = true;

    // Attempt to finalize the script in 1 second
    GetEntity()->AddCallbackTimer(1.0f, [this]() {
        AttemptToFinalize();
    });
}

void LuaScript::AttemptToFinalize()
{
    if (m_Hooks != 0) {
        m_AttemptingToFinalize = false;
        return;
    }

    switch (m_TerminateBehavior)
    {
        case LuaTerminateBehavior::ResetScriptAndDelete:
            Game::logger->Log("LuaScript", "Deleting script\n");
            delete this;
            break;
    }
}