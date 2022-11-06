#include "AgLaserSensorServer.h"

#include "PhantomPhysicsComponent.h"
#include "SkillComponent.h"
#include "EntityManager.h"
#include "AgMonumentLaserServer.h"
#include "EntityManager.h"

void AgLaserSensorServer::OnStartup(Entity* self) {

	PhantomPhysicsComponent* physComp = static_cast<PhantomPhysicsComponent*>(self->GetComponent(COMPONENT_TYPE_PHANTOM_PHYSICS));
	physComp->SetPhysicsEffectActive(true);
	physComp->SetEffectType(2); // repulse (prolly should make definitions of these are in Entity.cpp)
	physComp->SetDirectionalMultiplier(static_cast<float>(m_RepelForce));
	physComp->SetDirection(NiPoint3::UNIT_Y);

	m_Skill = self->GetComponent<SkillComponent>();
}


void AgLaserSensorServer::OnCollisionPhantom(Entity* self, Entity* target) {

	if (!m_Skill) return;


	Entity* laser = nullptr;

	for (auto script : EntityManager::Instance()->GetEntitiesByComponent(COMPONENT_TYPE_SCRIPT)) {

		AgMonumentLaserServer* hasLaser = (AgMonumentLaserServer*)script;

		if (hasLaser) {
			const auto source = script->GetPosition();
			const auto obj = self->GetObjectID();

			if (obj == 76690936093053 && Vector3::DistanceSquared(source, NiPoint3(149.007f, 417.083f, 218.346f)) <= 1.0f) {
				laser = script;
				break;
			} else if (obj == 75866302318824 && Vector3::DistanceSquared(source, NiPoint3(48.6403f, 403.803f, 196.711f)) <= 1.0f) {
				laser = script;
				break;
			} else if (obj == 75866302318822 && Vector3::DistanceSquared(source, NiPoint3(19.2155f, 420.083f, 249.226f)) <= 1.0f) {
				laser = script;
				break;
			} else if (obj == 75866302318823 && Vector3::DistanceSquared(source, NiPoint3(-6.61596f, 404.633f, 274.323f)) <= 1.0f) {
				laser = script;
				break;
			}
		}
	}

	if (laser != nullptr) {
		m_Skill->CalculateBehavior(m_SkillCastID, 15714, target->GetObjectID());
	}
}
