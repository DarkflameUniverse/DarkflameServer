#include "AgLaserSensorServer.h"

#include "PhantomPhysicsComponent.h"
#include "SkillComponent.h"
#include "ePhysicsEffectType.h"

void AgLaserSensorServer::OnStartup(Entity* self) {
	self->SetBoolean(u"active", true);
	auto repelForce = self->GetVarAs<float>(u"repelForce");
	if (!repelForce) repelForce = m_RepelForce;
	auto* phantomPhysicsComponent = self->GetComponent<PhantomPhysicsComponent>();
	if (!phantomPhysicsComponent) return;
	phantomPhysicsComponent->SetPhysicsEffectActive(true);
	phantomPhysicsComponent->SetEffectType(ePhysicsEffectType::REPULSE);
	phantomPhysicsComponent->SetDirectionalMultiplier(repelForce);
	phantomPhysicsComponent->SetDirection(NiPoint3::UNIT_Y);
}


void AgLaserSensorServer::OnCollisionPhantom(Entity* self, Entity* target) {
	auto active = self->GetVar<bool>(u"active");
	if (!active) return;
	auto skillCastID = self->GetVarAs<float>(u"skillCastID");
	if (!skillCastID) skillCastID = m_SkillCastID;
	auto* skillComponent = self->GetComponent<SkillComponent>();
	if (!skillComponent) return;
	skillComponent->CastSkill(m_SkillCastID, target->GetObjectID());
}
