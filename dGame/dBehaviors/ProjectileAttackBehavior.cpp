#include "ProjectileAttackBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "EntityManager.h"
#include "Game.h"
#include "dLogger.h"
#include "SkillComponent.h"
#include "../dWorldServer/ObjectIDManager.h"

void ProjectileAttackBehavior::Handle(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	LWOOBJID target;

	bitStream->Read(target);

	auto* entity = EntityManager::Instance()->GetEntity(context->originator);

	if (entity == nullptr) {
		Game::logger->Log("ProjectileAttackBehavior", "Failed to find originator (%llu)!", context->originator);

		return;
	}

	auto* skillComponent = entity->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		Game::logger->Log("ProjectileAttackBehavior", "Failed to find skill component for (%llu)!", -context->originator);

		return;
	}

	if (m_useMouseposit) {
		NiPoint3 targetPosition = NiPoint3::ZERO;
		bitStream->Read(targetPosition);
	}

	auto* targetEntity = EntityManager::Instance()->GetEntity(target);

	for (auto i = 0u; i < this->m_projectileCount; ++i) {
		LWOOBJID projectileId;

		bitStream->Read(projectileId);

		branch.target = target;
		branch.isProjectile = true;
		branch.referencePosition = targetEntity == nullptr ? entity->GetPosition() : targetEntity->GetPosition();

		skillComponent->RegisterPlayerProjectile(projectileId, context, branch, this->m_lot);
	}
}

void ProjectileAttackBehavior::Calculate(BehaviorContext* context, RakNet::BitStream* bitStream, BehaviorBranchContext branch) {
	bitStream->Write(branch.target);

	auto* entity = EntityManager::Instance()->GetEntity(context->originator);

	if (entity == nullptr) {
		Game::logger->Log("ProjectileAttackBehavior", "Failed to find originator (%llu)!", context->originator);

		return;
	}

	auto* skillComponent = entity->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		Game::logger->Log("ProjectileAttackBehavior", "Failed to find skill component for (%llu)!", context->originator);

		return;

	}

	auto* other = EntityManager::Instance()->GetEntity(branch.target);

	if (other == nullptr) {
		Game::logger->Log("ProjectileAttackBehavior", "Invalid projectile target (%llu)!", branch.target);

		return;
	}

	const auto position = entity->GetPosition() + this->m_offset;

	const auto distance = Vector3::Distance(position, other->GetPosition());

	const auto time = distance / this->m_projectileSpeed;

	const auto rotation = NiQuaternion::LookAtUnlocked(position, other->GetPosition());

	const auto targetPosition = other->GetPosition();

	//entity->SetRotation(rotation);

	const auto angleDelta = this->m_spreadAngle;
	const auto angleStep = angleDelta / this->m_projectileCount;

	auto angle = -angleStep;

	const auto maxTime = this->m_maxDistance / this->m_projectileSpeed;

	for (auto i = 0u; i < this->m_projectileCount; ++i) {
		auto id = static_cast<LWOOBJID>(ObjectIDManager::Instance()->GenerateObjectID());

		id = GeneralUtils::SetBit(id, OBJECT_BIT_CLIENT);

		bitStream->Write(id);

		auto eulerAngles = rotation.GetEulerAngles();

		eulerAngles.y += angle * (3.14 / 180);

		const auto angledRotation = NiQuaternion::FromEulerAngles(eulerAngles);

		const auto direction = angledRotation.GetForwardVector();

		const auto destination = position + direction * distance;

		branch.isProjectile = true;
		branch.referencePosition = destination;

		skillComponent->RegisterCalculatedProjectile(id, context, branch, this->m_lot, maxTime, position, direction * this->m_projectileSpeed, this->m_trackTarget, this->m_trackRadius);

		// No idea how to calculate this properly
		if (this->m_projectileCount == 2) {
			angle += angleDelta;
		} else if (this->m_projectileCount == 3) {
			angle += angleStep;
		}
	}
}

void ProjectileAttackBehavior::Load() {
	this->m_lot = GetInt("LOT_ID");

	this->m_projectileCount = GetInt("spread_count");

	if (this->m_projectileCount == 0) {
		this->m_projectileCount = 1;
	}

	this->m_maxDistance = GetFloat("max_distance");

	this->m_projectileSpeed = GetFloat("projectile_speed");

	this->m_spreadAngle = GetFloat("spread_angle");

	this->m_offset = { GetFloat("offset_x"), GetFloat("offset_y"), GetFloat("offset_z") };

	this->m_trackTarget = GetBoolean("track_target");

	this->m_trackRadius = GetFloat("track_radius");

	this->m_useMouseposit = GetBoolean("use_mouseposit");
}
