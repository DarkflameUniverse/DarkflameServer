#include "ChangeOrientationBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"

void ChangeOrientationBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	Entity* sourceEntity;
	if (this->m_orientCaster) sourceEntity = EntityManager::Instance()->GetEntity(context->originator);
	else sourceEntity = EntityManager::Instance()->GetEntity(branch.target);
	if (!sourceEntity) return;

	if (this->m_toTarget) {
		Entity* destinationEntity;
		if (this->m_orientCaster) destinationEntity = EntityManager::Instance()->GetEntity(branch.target);
		else destinationEntity = EntityManager::Instance()->GetEntity(context->originator);
		if (!destinationEntity) return;

		const auto source = sourceEntity->GetPosition();
		const auto destination = destinationEntity->GetPosition();
		sourceEntity->SetRotation(NiQuaternion::LookAt(source, destination));
	} else if (this->m_toAngle){
		auto baseAngle = NiPoint3(this->m_angle, 0, 0);
		if (this->m_relative){
			auto sourceAngle = sourceEntity->GetRotation().GetEulerAngles();
			baseAngle += sourceAngle;
		}
		auto newRotation = NiQuaternion::FromEulerAngles(baseAngle);
		sourceEntity->SetRotation(newRotation);
	} else return;
	EntityManager::Instance()->SerializeEntity(sourceEntity);
	return;
}

void ChangeOrientationBehavior::Load() {
	this->m_orientCaster = GetBoolean("orient_caster", true);
	this->m_toTarget = GetBoolean("to_target", false);
	this->m_toAngle = GetBoolean("to_angle", false);
	this->m_angle = GetFloat("angle", 0.0f);
	this->m_relative = GetBoolean("relative", false);
}
