#include "ChangeOrientationBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"

void ChangeOrientationBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Entity* sourceEntity;
	if (this->m_orientCaster) sourceEntity = Game::entityManager->GetEntity(context->originator);
	else sourceEntity = Game::entityManager->GetEntity(branch.target);
	if (!sourceEntity) return;

	if (this->m_toTarget) {
		Entity* destinationEntity;
		if (this->m_orientCaster) destinationEntity = Game::entityManager->GetEntity(branch.target);
		else destinationEntity = Game::entityManager->GetEntity(context->originator);
		if (!destinationEntity) return;

		sourceEntity->SetRotation(
			NiQuaternion::LookAt(sourceEntity->GetPosition(), destinationEntity->GetPosition())
		);
	} else if (this->m_toAngle){
		auto baseAngle = NiPoint3(0, 0, this->m_angle);
		if (this->m_relative) baseAngle += sourceEntity->GetRotation().GetForwardVector();
		sourceEntity->SetRotation(NiQuaternion::FromEulerAngles(baseAngle));
	} else return;
	Game::entityManager->SerializeEntity(sourceEntity);
	return;
}

void ChangeOrientationBehavior::Load() {
	this->m_orientCaster = GetBoolean("orient_caster", true);
	this->m_toTarget = GetBoolean("to_target", false);
	this->m_toAngle = GetBoolean("to_angle", false);
	this->m_angle = GetFloat("angle", 0.0f);
	this->m_relative = GetBoolean("relative", false);
}
