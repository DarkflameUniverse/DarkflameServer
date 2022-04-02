#pragma once

#include "CppScripts.h"
#include "Entity.h"
#include "EntityManager.h"

#include <gravity_compiler.h>
#include <gravity_macros.h>
#include <gravity_core.h>
#include <gravity_vm.h>
#include <gravity_vmmacros.h>

class GravityEntity {
public:
    GravityEntity(Entity* entity);
    ~GravityEntity();

    void CreateEntityGravityValue(gravity_vm* vm);
    bool GetObjectID(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex);

    Entity* GetEntity() { return m_Entity; }
    gravity_value_t GetGravityValue() { return m_GravityValue; }

private:
    Entity* m_Entity = nullptr;
    gravity_value_t m_GravityValue;
};