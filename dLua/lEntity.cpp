#include "lEntity.h"

#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "LuaScript.h"
#include "DestroyableComponent.h"
#include "BaseCombatAIComponent.h"
#include "MovementAIComponent.h"
#include "GameMessages.h"
#include "EntityInfo.h"

void lEntity::RegisterClass(LuaScript* script)
{
    sol::state& lua = script->GetState();

    lua.new_usertype<lEntity>("Entity",
        sol::constructors<lEntity(LWOOBJID)>(),
        "IsValid", &lEntity::IsValid,
        "GetID", &lEntity::GetID,
        "GetLOT", &lEntity::GetLOT,
        "GetPosition", &lEntity::GetPosition,
        "GetRotation", &lEntity::GetRotation,
        "SetPosition", &lEntity::SetPosition,
        "SetRotation", &lEntity::SetRotation,
        "AddTimer", &lEntity::AddTimer,
        "Smash", &lEntity::Smash,
        "LoadScript", &lEntity::LoadScript,
        "SetVar", &lEntity::SetVar,
        "GetVar", [script](lEntity& entity, const std::string& key) {
            return entity.GetVar(script, key);
        },

        #pragma region DestroyableComponent
        "GetHealth", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetHealth();
        },
        "GetArmor", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetArmor();
        },
        "GetImagination", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetImagination();
        },
        "GetMaxHealth", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetMaxHealth();
        },
        "GetMaxArmor", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetMaxArmor();
        },
        "GetMaxImagination", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetMaxImagination();
        },
        "SetHealth", [](lEntity& entity, int32_t health) {
            GetComponent<DestroyableComponent>(entity)->SetHealth(health);
        },
        "SetArmor", [](lEntity& entity, int32_t armor) {
            GetComponent<DestroyableComponent>(entity)->SetArmor(armor);
        },
        "SetImagination", [](lEntity& entity, int32_t imagination) {
            GetComponent<DestroyableComponent>(entity)->SetImagination(imagination);
        },
        "SetMaxHealth", [](lEntity& entity, float maxHealth) {
            GetComponent<DestroyableComponent>(entity)->SetMaxHealth(maxHealth);
        },
        "SetMaxArmor", [](lEntity& entity, float maxArmor) {
            GetComponent<DestroyableComponent>(entity)->SetMaxArmor(maxArmor);
        },
        "SetMaxImagination", [](lEntity& entity, float maxImagination) {
            GetComponent<DestroyableComponent>(entity)->SetMaxImagination(maxImagination);
        },
        "Damage", [](lEntity& entity, int32_t damage, sol::object origin) {
            GetComponent<DestroyableComponent>(entity)->Damage(damage, dLua::GetEntityID(origin));
        },
        "Heal", [](lEntity& entity, int32_t heal) {
            GetComponent<DestroyableComponent>(entity)->Heal(heal);
        },
        "Repair", [](lEntity& entity, int32_t repair) {
            GetComponent<DestroyableComponent>(entity)->Repair(repair);
        },
        "Imagine", [](lEntity& entity, int32_t imagine) {
            GetComponent<DestroyableComponent>(entity)->Imagine(imagine);
        },
        "GetDamageToAbsorb", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetDamageToAbsorb();
        },
        "SetDamageToAbsorb", [](lEntity& entity, int32_t damageToAbsorb) {
            GetComponent<DestroyableComponent>(entity)->SetDamageToAbsorb(damageToAbsorb);
        },
        "GetDamageReduction", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetDamageReduction();
        },
        "SetDamageReduction", [](lEntity& entity, int32_t damageReduction) {
            GetComponent<DestroyableComponent>(entity)->SetDamageReduction(damageReduction);
        },
        "GetIsImmune", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->IsImmune();
        },
        "SetIsImmune", [](lEntity& entity, bool immune) {
            GetComponent<DestroyableComponent>(entity)->SetIsImmune(immune);
        },
        "GetIsShielded", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetIsShielded();
        },
        "SetIsShielded", [](lEntity& entity, bool shielded) {
            GetComponent<DestroyableComponent>(entity)->SetIsShielded(shielded);
        },
        "AddFaction", [](lEntity& entity, int32_t faction) {
            GetComponent<DestroyableComponent>(entity)->AddFaction(faction);
        },
        "AddEnemyFaction", [](lEntity& entity, int32_t faction) {
            GetComponent<DestroyableComponent>(entity)->AddEnemyFaction(faction);
        },
        "GetAttacksToBlock", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetAttacksToBlock();
        },
        "SetAttacksToBlock", [](lEntity& entity, int32_t attacksToBlock) {
            GetComponent<DestroyableComponent>(entity)->SetAttacksToBlock(attacksToBlock);
        },
        "SetFaction", [](lEntity& entity, int32_t faction) {
            GetComponent<DestroyableComponent>(entity)->SetFaction(faction);
        },
        "HasFaction", [](lEntity& entity, int32_t faction) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->HasFaction(faction);
        },
        "GetLootMatrixID", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetLootMatrixID();
        },
        "SetLootMatrixID", [](lEntity& entity, int32_t lootMatrixID) {
            GetComponent<DestroyableComponent>(entity)->SetLootMatrixID(lootMatrixID);
        },
        "GetIsSmashable", [](lEntity& entity) {
            AssertComponent<DestroyableComponent>(entity);
            return entity->GetComponent<DestroyableComponent>()->GetIsSmashable();
        },
        "SetIsSmashable", [](lEntity& entity, bool smashable) {
            GetComponent<DestroyableComponent>(entity)->SetIsSmashable(smashable);
        },

        #pragma endregion DestroyableComponent

        #pragma region BaseCombatAIComponent
        "GetStunned", [](lEntity& entity) {
            AssertComponent<BaseCombatAIComponent>(entity);
            return entity->GetComponent<BaseCombatAIComponent>()->GetStunned();
        },
        "SetStunned", [](lEntity& entity, bool stunned) {
            AssertComponent<BaseCombatAIComponent>(entity);
            entity->GetComponent<BaseCombatAIComponent>()->SetStunned(stunned);
        },
        "Stun", [](lEntity& entity, float duration) {
            AssertComponent<BaseCombatAIComponent>(entity);
            entity->GetComponent<BaseCombatAIComponent>()->Stun(duration);
        },
        "GetAIDisabled", [](lEntity& entity) {
            AssertComponent<BaseCombatAIComponent>(entity);
            return entity->GetComponent<BaseCombatAIComponent>()->GetDistabled();
        },
        "SetAIDisabled", [](lEntity& entity, bool disabled) {
            AssertComponent<BaseCombatAIComponent>(entity);
            entity->GetComponent<BaseCombatAIComponent>()->SetDisabled(disabled);
        },
        #pragma endregion BaseCombatAIComponent

        #pragma region MovementAIComponent

        "GetDestination", [](lEntity& entity) {
            GetComponent<MovementAIComponent>(entity)->GetDestination();
        },
        "SetDestination", [](lEntity& entity, const NiPoint3& destination) {
            GetComponent<MovementAIComponent>(entity)->SetDestination(destination);
        },
        "Warp", [](lEntity& entity, const NiPoint3& point) {
            GetComponent<MovementAIComponent>(entity)->Warp(point);
        },

        #pragma endregion MovementAIComponent

        #pragma region GameMessages
        "PlayAnimation", [](lEntity& entity, const std::string& animation) {
            GameMessages::SendPlayAnimation(entity.GetEntityOrThrow(), GeneralUtils::ASCIIToUTF16(animation));
        },
        "PlayFX", [](lEntity& entity, int32_t effectID, const std::string& effectType, const std::string& name) {
            GameMessages::SendPlayFXEffect(entity.GetID(), effectID, GeneralUtils::ASCIIToUTF16(effectType), name);
        },
        "StopFX", [](lEntity& entity, const std::string& name, bool killImmediate) {
            GameMessages::SendStopFXEffect(entity.GetEntityOrThrow(), killImmediate, name);
        }
        #pragma endregion GameMessages
    );

    // Register a function create a new entity
    // Takes a table with the following fields:
    // - lot: number
    // - position: Vector3 (optional)
    // - rotation: Quaternion (optional)
    // - spawner: lEntity (optional)
    lua.set_function("spawn", [script](sol::table entityData) {
        sol::state& lua = script->GetState();

        // Get the lot
        LOT lot = dLua::TableGet<LOT>(entityData, "lot");

        // Use the state entity's position and rotation if none is provided
        Entity* stateEntity = script->GetEntity();

        // Get the position
        NiPoint3 position;

        if (!dLua::TryTableGet(entityData, "position", position))
            position = stateEntity->GetPosition();

        // Get the rotation
        NiQuaternion rotation;

        if (!dLua::TryTableGet(entityData, "rotation", rotation))
            rotation = stateEntity->GetRotation();

        // Get the spawner
        Entity* spawner = nullptr;
        
        lEntity spawnerEntity;
        if (dLua::TryTableGet(entityData, "spawner", spawnerEntity)) {
            spawner = dLua::GetEntityOrThrow(spawnerEntity.id);
        } else {
            spawner = stateEntity;
        }

        // Create the entity
        EntityInfo info {};
        info.lot = lot;
        info.pos = position;
        info.rot = rotation;
        info.spawnerID = spawner->GetObjectID();

        Entity* entity = EntityManager::Instance()->CreateEntity(info);

        EntityManager::Instance()->ConstructEntity(entity);

        // Return the entity
        return lEntity(entity->GetObjectID());
    });
}

lEntity::lEntity(Entity* entity) {
    if (entity == nullptr) {
        this->id = LWOOBJID_EMPTY;
    } else {
        this->id = entity->GetObjectID();
    }
}

bool lEntity::IsValid() const {
    return GetEntity() != nullptr;    
}

LWOOBJID lEntity::GetID() const {
    return id;
}

LOT lEntity::GetLOT() const {
    return GetEntityOrThrow()->GetLOT();
}

NiPoint3 lEntity::GetPosition() const {
    return GetEntityOrThrow()->GetPosition();
}

NiQuaternion lEntity::GetRotation() const {
    return GetEntityOrThrow()->GetRotation();
}

void lEntity::SetPosition(const NiPoint3& pos) {
    GetEntityOrThrow()->SetPosition(pos);
}

void lEntity::SetRotation(const NiQuaternion& rot) {
    GetEntityOrThrow()->SetRotation(rot);
}

void lEntity::SetVar(const std::string& key, sol::object value) {
    const std::u16string key16 = GeneralUtils::ASCIIToUTF16(key);

    Entity* entity = GetEntityOrThrow();

    sol::type type = value.get_type();

    if (value.is<lEntity>()) {
        LWOOBJID entityID = value.as<lEntity>().GetID();

        entity->SetVar(key16, entityID);

        return;
    }
    
    switch (type) {
        case sol::type::boolean:
            entity->SetVar(key16, value.as<bool>());
            break;
        case sol::type::number:
            entity->SetVar(key16, value.as<float>());
            break;
        case sol::type::string:
            entity->SetVar(key16, GeneralUtils::ASCIIToUTF16(value.as<std::string>()));
            break;
        case sol::type::nil:
            entity->SetVar(key16, value); // Maybe store nil some other way?
            break;
        default:
            Game::logger->Log("lEntity", "Unknown type for SetVar: %i\n", type);
            entity->SetVar(key16, value);
            break;
    }
}

sol::object lEntity::GetVar(LuaScript* script, const std::string& key) {
    const std::u16string key16 = GeneralUtils::ASCIIToUTF16(key);

    Entity* entity = GetEntityOrThrow();

    LDFBaseData* value = entity->GetVarData(key16);

    if (value == nullptr) {
        return sol::make_object(script->GetState(), sol::nil);
    }

    eLDFType type = value->GetValueType();

    sol::state& lua = script->GetState();

   switch (type) {
        case LDF_TYPE_UTF_16:
            return sol::make_object(lua, GeneralUtils::UTF16ToWTF8(entity->GetVar<std::u16string>(key16)));
        case LDF_TYPE_S32:
            return sol::make_object(lua, entity->GetVar<int32_t>(key16));
        case LDF_TYPE_FLOAT:
            return sol::make_object(lua, entity->GetVar<float>(key16));
        case LDF_TYPE_DOUBLE:
            return sol::make_object(lua, entity->GetVar<double>(key16));
        case LDF_TYPE_U32:
            return sol::make_object(lua, entity->GetVar<uint32_t>(key16));
        case LDF_TYPE_BOOLEAN:
            return sol::make_object(lua, entity->GetVar<bool>(key16));
        case LDF_TYPE_U64:
            return sol::make_object(lua, entity->GetVar<uint64_t>(key16));
        case LDF_TYPE_OBJID:
            return sol::make_object(lua, lEntity(entity->GetVar<LWOOBJID>(key16)));
        case LDF_TYPE_UTF_8:
            return sol::make_object(lua, entity->GetVar<std::string>(key16));
        default:
            Game::logger->Log("lEntity", "Unknown type for GetVar: %i\n", type);
            return entity->GetVar<sol::object>(key16);
    }
}

void lEntity::Smash(sol::object smasher) {
    Entity* entity = GetEntityOrThrow();

    LWOOBJID smasherEntity = dLua::GetEntityID(smasher);

    entity->Smash(smasherEntity);
}

void lEntity::AddTimer(const std::string& name, float seconds) {
    GetEntityOrThrow()->AddTimer(name, seconds);
}

void lEntity::LoadScript(const std::string& name) {
    dLua::LoadScript(GetEntityOrThrow(), name);
}

void lEntity::Serialize() {
    EntityManager::Instance()->SerializeEntity(GetEntityOrThrow());
}

Entity* lEntity::GetEntity() const
{
    return dLua::GetEntity(id);
}

Entity* lEntity::GetEntityOrThrow() const
{
    return dLua::GetEntityOrThrow(id);
}

Entity* lEntity::operator->() const
{
    return GetEntity();
}
