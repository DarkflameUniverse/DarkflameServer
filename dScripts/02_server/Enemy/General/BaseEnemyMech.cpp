#include "BaseEnemyMech.h"
#include "Entity.h"
#include "ControllablePhysicsComponent.h"
#include "EntityManager.h"
#include "dpWorld.h"
#include "GeneralUtils.h"
#include "DestroyableComponent.h"

void BaseEnemyMech::OnStartup(Entity* self) {
	auto* destroyableComponent = self->GetComponent<DestroyableComponent>();
	if (destroyableComponent != nullptr) {
		destroyableComponent->SetFaction(4);
	}
}

void BaseEnemyMech::OnDie(Entity* self, Entity* killer) {
	ControllablePhysicsComponent* controlPhys = static_cast<ControllablePhysicsComponent*>(self->GetComponent(COMPONENT_TYPE_CONTROLLABLE_PHYSICS));
	if (!controlPhys) return;

	NiPoint3 newLoc = { controlPhys->GetPosition().x, dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(controlPhys->GetPosition()), controlPhys->GetPosition().z };

	EntityInfo info = EntityInfo();
	std::vector<LDFBaseData*> cfg;
	std::u16string activatorPosStr;
	activatorPosStr += (GeneralUtils::to_u16string(controlPhys->GetPosition().x));
	activatorPosStr.push_back(0x1f);
	activatorPosStr += (GeneralUtils::to_u16string(controlPhys->GetPosition().y));
	activatorPosStr.push_back(0x1f);
	activatorPosStr += (GeneralUtils::to_u16string(controlPhys->GetPosition().z));

	LDFBaseData* activatorPos = new LDFData<std::u16string>(u"rebuild_activators", activatorPosStr);
	cfg.push_back(activatorPos);
	info.lot = qbTurretLOT;
	info.pos = newLoc;
	info.rot = controlPhys->GetRotation();
	info.spawnerID = self->GetObjectID();
	info.settings = cfg;

	Entity* turret = EntityManager::Instance()->CreateEntity(info, nullptr);
	if (turret) {
		EntityManager::Instance()->ConstructEntity(turret);
	}
}
