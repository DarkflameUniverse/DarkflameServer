#include "ActNinjaRebuild.h"
#include "EntityInfo.h"
#include "ControllablePhysicsComponent.h"

void ActNinjaRebuild::OnStartup(Entity* self){
	EntityInfo info{};
	info.lot = m_Base;
	info.pos = self->GetPosition();
	info.rot = self->GetRotation();
	info.spawnerID = self->GetObjectID();
	m_BaseEntity = EntityManager::Instance()->CreateEntity(info);

	EntityManager::Instance()->ConstructEntity(m_BaseEntity);

	auto* controllablePhysicsComponent = self->GetComponent<ControllablePhysicsComponent>();
	if (controllablePhysicsComponent) {
		controllablePhysicsComponent->SetStunImmunity(
			eStateChangeType::POP, self->GetObjectID(), true, true, true, true, true, true, true
		);
	}
}

void ActNinjaRebuild::OnDie(Entity* self, Entity* killer){
	if (m_BaseEntity) m_BaseEntity->Smash(killer ? killer->GetObjectID() : LWOOBJID_EMPTY, eKillType::SILENT);
}
