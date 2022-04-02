#include "GravityEntity.h"

// Helper functions

Entity* GetEntityFromArgs(gravity_vm* vm, gravity_value_t* args, uint16_t nargs) {
    if (!(nargs > 0)) {
        return nullptr;
    }

    if (!(VALUE_ISA_CLASS(args[0]))) {
        return nullptr;
    }

    auto* entity = VALUE_AS_OBJECT(args[0]);
    LWOOBJID objId = std::stoull(entity->identifier);

    return EntityManager::Instance()->GetEntity(objId);
}

bool GravityEntity::GetObjectID(gravity_vm *vm, gravity_value_t *args, uint16_t nargs, uint32_t rindex) {
    RETURN_VALUE(VALUE_FROM_INT(m_Entity->GetObjectID()), rindex);
}

void GravityEntity::CreateEntityGravityValue(gravity_vm* vm) {
    gravity_gc_setenabled(vm, false);

    gravity_class_t* class_ = gravity_class_new_pair(vm, "Entity", NULL, 0, 0);

    // Generate bindings for all functions

    // GetObjectID
    gravity_class_bind(gravity_class_get_meta(class_), "GetObjectID", NEW_CLOSURE_VALUE(this->GetObjectID));

    m_GravityValue = VALUE_FROM_OBJECT(class_);

    gravity_gc_setenabled(vm, true);
}

GravityEntity::GravityEntity(Entity* entity) {
    m_Entity = entity;
}

GravityEntity::~GravityEntity() {
    m_Entity = nullptr;
}



