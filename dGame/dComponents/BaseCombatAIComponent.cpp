#include "BaseCombatAIComponent.h"
#include <BitStream.h>

#include "Entity.h"
#include "EntityManager.h"
#include "ControllablePhysicsComponent.h"
#include "MovementAIComponent.h"
#include "dpWorld.h"

#include "GameMessages.h"
#include "dServer.h"
#include "Game.h"

#include "CDClientDatabase.h"
#include "CDClientManager.h"
#include "DestroyableComponent.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "SkillComponent.h"
#include "RebuildComponent.h"
#include "DestroyableComponent.h"

BaseCombatAIComponent::BaseCombatAIComponent(Entity* parent, const uint32_t id) : Component(parent) {
	m_Target = LWOOBJID_EMPTY;
	m_State = AiState::spawn;
	m_Timer = 1.0f;
	m_StartPosition = parent->GetPosition();
	m_MovementAI = nullptr;
	m_Disabled = false;
	m_SkillEntries = {};
	m_MovementAI = nullptr;
	m_SoftTimer = 5.0f;

	//Grab the aggro information from BaseCombatAI:
	auto componentQuery = CDClientDatabase::CreatePreppedStmt(
		"SELECT aggroRadius, tetherSpeed, pursuitSpeed, softTetherRadius, hardTetherRadius FROM BaseCombatAIComponent WHERE id = ?;");
	componentQuery.bind(1, (int)id);

	auto componentResult = componentQuery.execQuery();

	if (!componentResult.eof()) {
		if (!componentResult.fieldIsNull(0))
			m_AggroRadius = componentResult.getFloatField(0);

		if (!componentResult.fieldIsNull(1))
			m_TetherSpeed = componentResult.getFloatField(1);

		if (!componentResult.fieldIsNull(2))
			m_PursuitSpeed = componentResult.getFloatField(2);

		if (!componentResult.fieldIsNull(3))
			m_SoftTetherRadius = componentResult.getFloatField(3);

		if (!componentResult.fieldIsNull(4))
			m_HardTetherRadius = componentResult.getFloatField(4);
	}

	componentResult.finalize();

	// Get aggro and tether radius from settings and use this if it is present.  Only overwrite the
	// radii if it is greater than the one in the database.
	if (m_Parent) {
		auto aggroRadius = m_Parent->GetVar<float>(u"aggroRadius");
		m_AggroRadius = aggroRadius != 0 ? aggroRadius : m_AggroRadius;
		auto tetherRadius = m_Parent->GetVar<float>(u"tetherRadius");
		m_HardTetherRadius = tetherRadius != 0 ? tetherRadius : m_HardTetherRadius;
	}

	/*
	 * Find skills
	 */
	auto skillQuery = CDClientDatabase::CreatePreppedStmt(
		"SELECT skillID, cooldown, behaviorID FROM SkillBehavior WHERE skillID IN (SELECT skillID FROM ObjectSkills WHERE objectTemplate = ?);");
	skillQuery.bind(1, (int)parent->GetLOT());

	auto result = skillQuery.execQuery();

	while (!result.eof()) {
		const auto skillId = static_cast<uint32_t>(result.getIntField(0));

		const auto abilityCooldown = static_cast<float>(result.getFloatField(1));

		const auto behaviorId = static_cast<uint32_t>(result.getIntField(2));

		auto* behavior = Behavior::CreateBehavior(behaviorId);

		std::stringstream behaviorQuery;

		AiSkillEntry entry = { skillId, 0, abilityCooldown, behavior };

		m_SkillEntries.push_back(entry);

		result.nextRow();
	}

	Stun(1.0f);

	/*
	 * Add physics
	 */

	int32_t collisionGroup = (COLLISION_GROUP_DYNAMIC | COLLISION_GROUP_ENEMY);

	CDComponentsRegistryTable* componentRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
	auto componentID = componentRegistryTable->GetByIDAndType(parent->GetLOT(), COMPONENT_TYPE_CONTROLLABLE_PHYSICS);

	CDPhysicsComponentTable* physicsComponentTable = CDClientManager::Instance()->GetTable<CDPhysicsComponentTable>("PhysicsComponent");

	if (physicsComponentTable != nullptr) {
		auto* info = physicsComponentTable->GetByID(componentID);
		if (info != nullptr) {
			collisionGroup = info->bStatic ? COLLISION_GROUP_NEUTRAL : info->collisionGroup;
		}
	}

	//Create a phantom physics volume so we can detect when we're aggro'd.
	m_dpEntity = new dpEntity(m_Parent->GetObjectID(), m_AggroRadius);
	m_dpEntityEnemy = new dpEntity(m_Parent->GetObjectID(), m_AggroRadius, false);

	m_dpEntity->SetCollisionGroup(collisionGroup);
	m_dpEntityEnemy->SetCollisionGroup(collisionGroup);

	m_dpEntity->SetPosition(m_Parent->GetPosition());
	m_dpEntityEnemy->SetPosition(m_Parent->GetPosition());

	dpWorld::Instance().AddEntity(m_dpEntity);
	dpWorld::Instance().AddEntity(m_dpEntityEnemy);

}

BaseCombatAIComponent::~BaseCombatAIComponent() {
	if (m_dpEntity)
		dpWorld::Instance().RemoveEntity(m_dpEntity);

	if (m_dpEntityEnemy)
		dpWorld::Instance().RemoveEntity(m_dpEntityEnemy);
}

void BaseCombatAIComponent::Update(const float deltaTime) {
	//First, we need to process physics:
	if (!m_dpEntity) return;

	m_dpEntity->SetPosition(m_Parent->GetPosition()); //make sure our position is synced with our dpEntity
	m_dpEntityEnemy->SetPosition(m_Parent->GetPosition());

	//Process enter events
	for (auto en : m_dpEntity->GetNewObjects()) {
		m_Parent->OnCollisionPhantom(en->GetObjectID());
	}

	//Process exit events
	for (auto en : m_dpEntity->GetRemovedObjects()) {
		m_Parent->OnCollisionLeavePhantom(en->GetObjectID());
	}

	// Check if we should stop the tether effect
	if (m_TetherEffectActive) {
		m_TetherTime -= deltaTime;
		const auto& info = m_MovementAI->GetInfo();
		if (m_Target != LWOOBJID_EMPTY || (NiPoint3::DistanceSquared(
			m_StartPosition,
			m_Parent->GetPosition()) < 20 * 20 && m_TetherTime <= 0)
			) {
			GameMessages::SendStopFXEffect(m_Parent, true, "tether");
			m_TetherEffectActive = false;
		}
	}

	if (m_SoftTimer <= 0.0f) {
		EntityManager::Instance()->SerializeEntity(m_Parent);

		m_SoftTimer = 5.0f;
	} else {
		m_SoftTimer -= deltaTime;
	}

	if (m_Disabled || m_Parent->GetIsDead())
		return;

	CalculateCombat(deltaTime); // Putting this here for now

	if (m_StartPosition == NiPoint3::ZERO) {
		m_StartPosition = m_Parent->GetPosition();
	}

	m_MovementAI = m_Parent->GetComponent<MovementAIComponent>();

	if (m_MovementAI == nullptr) {
		return;
	}

	if (m_Stunned) {
		m_MovementAI->Stop();

		return;
	}

	if (m_Timer > 0.0f) {
		m_Timer -= deltaTime;
		return;
	}

	switch (m_State) {
	case AiState::spawn:
		Stun(2.0f);
		m_State = AiState::idle;
		break;

	case AiState::idle:
		Wander();
		break;

	case AiState::aggro:
		OnAggro();
		break;

	case AiState::tether:
		OnTether();
		break;

	default:
		break;
	}
}


void BaseCombatAIComponent::CalculateCombat(const float deltaTime) {
	auto* rebuild = m_Parent->GetComponent<RebuildComponent>();

	if (rebuild != nullptr) {
		const auto state = rebuild->GetState();

		if (state != REBUILD_COMPLETED) {
			return;
		}
	}

	auto* skillComponent = m_Parent->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) {
		return;
	}

	skillComponent->CalculateUpdate(deltaTime);

	if (m_Disabled) return;

	if (m_StunTime > 0.0f) {
		m_StunTime -= deltaTime;

		if (m_StunTime > 0.0f) {
			return;
		}

		m_Stunned = false;
	}

	if (m_Stunned) {
		return;
	}

	auto newTarget = FindTarget();

	// Tether - reset enemy
	if (m_Target != LWOOBJID_EMPTY && newTarget == LWOOBJID_EMPTY) {
		m_OutOfCombat = true;
		m_OutOfCombatTime = 1.0f;
	} else if (newTarget != LWOOBJID_EMPTY) {
		m_OutOfCombat = false;
		m_OutOfCombatTime = 0.0f;
	}

	if (!m_TetherEffectActive && m_OutOfCombat && (m_OutOfCombatTime -= deltaTime) <= 0) {
		auto* destroyableComponent = m_Parent->GetComponent<DestroyableComponent>();

		if (destroyableComponent != nullptr && destroyableComponent->HasFaction(4)) {
			auto serilizationRequired = false;

			if (destroyableComponent->GetHealth() != destroyableComponent->GetMaxHealth()) {
				destroyableComponent->SetHealth(destroyableComponent->GetMaxHealth());

				serilizationRequired = true;
			}

			if (destroyableComponent->GetArmor() != destroyableComponent->GetMaxArmor()) {
				destroyableComponent->SetArmor(destroyableComponent->GetMaxArmor());

				serilizationRequired = true;
			}

			if (serilizationRequired) {
				EntityManager::Instance()->SerializeEntity(m_Parent);
			}

			GameMessages::SendPlayFXEffect(m_Parent->GetObjectID(), 6270, u"tether", "tether");

			m_TetherEffectActive = true;

			m_TetherTime = 3.0f;
		}

		// Speed towards start position
		if (m_MovementAI != nullptr) {
			m_MovementAI->SetHaltDistance(0);
			m_MovementAI->SetSpeed(m_PursuitSpeed);
			m_MovementAI->SetDestination(m_StartPosition);
		}

		m_OutOfCombat = false;
		m_OutOfCombatTime = 0.0f;
	}

	SetTarget(newTarget);

	if (m_Target != LWOOBJID_EMPTY) {
		if (m_State == AiState::idle) {
			m_Timer = 0;
		}

		m_State = AiState::aggro;
	} else {
		m_State = AiState::idle;
	}

	for (auto i = 0; i < m_SkillEntries.size(); ++i) {
		auto entry = m_SkillEntries.at(i);

		if (entry.cooldown > 0) {
			entry.cooldown -= deltaTime;

			m_SkillEntries[i] = entry;
		}
	}

	if (m_SkillTime > 0) {
		m_SkillTime -= deltaTime;

		return;
	}

	if (m_Downtime > 0) {
		m_Downtime -= deltaTime;

		return;
	}

	if (m_Target == LWOOBJID_EMPTY) {
		m_State = AiState::idle;

		return;
	}

	m_Downtime = 0.5f;

	auto* target = GetTargetEntity();

	if (target != nullptr) {
		LookAt(target->GetPosition());
	}

	for (auto i = 0; i < m_SkillEntries.size(); ++i) {
		auto entry = m_SkillEntries.at(i);

		if (entry.cooldown > 0) {
			continue;
		}

		const auto result = skillComponent->CalculateBehavior(entry.skillId, entry.behavior->m_behaviorId, LWOOBJID_EMPTY);

		if (result.success) {
			if (m_MovementAI != nullptr) {
				m_MovementAI->Stop();
			}

			m_State = AiState::aggro;

			m_Timer = 0;

			m_SkillTime = result.skillTime;

			entry.cooldown = entry.abilityCooldown + m_SkillTime;

			m_SkillEntries[i] = entry;

			break;
		}
	}
}

LWOOBJID BaseCombatAIComponent::FindTarget() {
	//const auto reference = m_MovementAI == nullptr ? m_StartPosition : m_MovementAI->ApproximateLocation();

	NiPoint3 reference = m_StartPosition;

	if (m_MovementAI) reference = m_MovementAI->ApproximateLocation();

	auto* target = GetTargetEntity();

	if (target != nullptr && !m_DirtyThreat) {
		const auto targetPosition = target->GetPosition();

		if (Vector3::DistanceSquared(targetPosition, m_StartPosition) < m_HardTetherRadius * m_HardTetherRadius) {
			return m_Target;
		}

		return LWOOBJID_EMPTY;
	}

	auto possibleTargets = GetTargetWithinAggroRange();

	if (possibleTargets.empty() && m_ThreatEntries.empty()) {
		m_DirtyThreat = false;

		return LWOOBJID_EMPTY;
	}

	Entity* optimalTarget = nullptr;
	float biggestThreat = 0;

	for (const auto& entry : possibleTargets) {
		auto* entity = EntityManager::Instance()->GetEntity(entry);

		if (entity == nullptr) {
			continue;
		}

		const auto targetPosition = entity->GetPosition();

		const auto threat = GetThreat(entry);

		const auto maxDistanceSquared = m_HardTetherRadius * m_HardTetherRadius;

		if (Vector3::DistanceSquared(targetPosition, m_StartPosition) > maxDistanceSquared) {
			if (threat > 0) {
				SetThreat(entry, 0);
			}

			continue;
		}

		if (threat > biggestThreat) {
			biggestThreat = threat;
			optimalTarget = entity;

			continue;
		}

		const auto proximityThreat = -(Vector3::DistanceSquared(targetPosition, reference) - maxDistanceSquared) / 100; // Proximity threat takes last priority

		if (proximityThreat > biggestThreat) {
			biggestThreat = proximityThreat;
			optimalTarget = entity;
		}
	}

	if (!m_DirtyThreat) {
		if (optimalTarget == nullptr) {
			return LWOOBJID_EMPTY;
		} else {
			return optimalTarget->GetObjectID();
		}
	}

	std::vector<LWOOBJID> deadThreats{};

	for (const auto& threatTarget : m_ThreatEntries) {
		auto* entity = EntityManager::Instance()->GetEntity(threatTarget.first);

		if (entity == nullptr) {
			deadThreats.push_back(threatTarget.first);

			continue;
		}

		const auto targetPosition = entity->GetPosition();

		if (Vector3::DistanceSquared(targetPosition, m_StartPosition) > m_HardTetherRadius * m_HardTetherRadius) {
			deadThreats.push_back(threatTarget.first);

			continue;
		}

		if (threatTarget.second > biggestThreat) {
			optimalTarget = entity;
			biggestThreat = threatTarget.second;
		}
	}

	for (const auto& deadThreat : deadThreats) {
		SetThreat(deadThreat, 0);
	}

	m_DirtyThreat = false;

	if (optimalTarget == nullptr) {
		return LWOOBJID_EMPTY;
	} else {
		return optimalTarget->GetObjectID();
	}
}

std::vector<LWOOBJID> BaseCombatAIComponent::GetTargetWithinAggroRange() const {
	std::vector<LWOOBJID> targets;

	for (auto id : m_Parent->GetTargetsInPhantom()) {
		auto* other = EntityManager::Instance()->GetEntity(id);

		const auto distance = Vector3::DistanceSquared(m_Parent->GetPosition(), other->GetPosition());

		if (distance > m_AggroRadius * m_AggroRadius) continue;

		targets.push_back(id);
	}

	return targets;
}

bool BaseCombatAIComponent::IsMech() {
	switch (m_Parent->GetLOT()) {
	case 6253:
		return true;

	default:
		return false;
	}

	return false;
}


void BaseCombatAIComponent::Serialize(RakNet::BitStream* outBitStream, bool bIsInitialUpdate, unsigned int& flags) {
	outBitStream->Write1();
	outBitStream->Write(uint32_t(m_State));
	outBitStream->Write(m_Target);
}


bool BaseCombatAIComponent::IsEnemy(LWOOBJID target) const {
	auto* entity = EntityManager::Instance()->GetEntity(target);

	if (entity == nullptr) {
		Game::logger->Log("BaseCombatAIComponent", "Invalid entity for checking validity (%llu)!", target);

		return false;
	}

	auto* destroyable = entity->GetComponent<DestroyableComponent>();

	if (destroyable == nullptr) {
		return false;
	}

	auto* referenceDestroyable = m_Parent->GetComponent<DestroyableComponent>();

	if (referenceDestroyable == nullptr) {
		Game::logger->Log("BaseCombatAIComponent", "Invalid reference destroyable component on (%llu)!", m_Parent->GetObjectID());

		return false;
	}

	auto* quickbuild = entity->GetComponent<RebuildComponent>();

	if (quickbuild != nullptr) {
		const auto state = quickbuild->GetState();

		if (state != REBUILD_COMPLETED) {
			return false;
		}
	}

	auto enemyList = referenceDestroyable->GetEnemyFactionsIDs();

	auto candidateList = destroyable->GetFactionIDs();

	for (auto value : candidateList) {
		if (std::find(enemyList.begin(), enemyList.end(), value) != enemyList.end()) {
			return true;
		}
	}

	return false;
}

void BaseCombatAIComponent::SetTarget(const LWOOBJID target) {
	m_Target = target;
}

Entity* BaseCombatAIComponent::GetTargetEntity() const {
	return EntityManager::Instance()->GetEntity(m_Target);
}

void BaseCombatAIComponent::Taunt(LWOOBJID offender, float threat) {
	// Can't taunt self
	if (offender == m_Parent->GetObjectID())
		return;

	m_ThreatEntries[offender] += threat;
	m_DirtyThreat = true;
}

float BaseCombatAIComponent::GetThreat(LWOOBJID offender) {
	const auto pair = m_ThreatEntries.find(offender);

	if (pair == m_ThreatEntries.end()) return 0;

	return pair->second;
}

void BaseCombatAIComponent::SetThreat(LWOOBJID offender, float threat) {
	if (threat == 0) {
		m_ThreatEntries.erase(offender);
	} else {
		m_ThreatEntries[offender] = threat;
	}

	m_DirtyThreat = true;
}

const NiPoint3& BaseCombatAIComponent::GetStartPosition() const {
	return m_StartPosition;
}

void BaseCombatAIComponent::ClearThreat() {
	m_ThreatEntries.clear();

	m_DirtyThreat = true;
}

void BaseCombatAIComponent::Wander() {
	if (!m_MovementAI->AtFinalWaypoint()) {
		return;
	}

	m_MovementAI->SetHaltDistance(0);

	const auto& info = m_MovementAI->GetInfo();

	const auto div = static_cast<int>(info.wanderDelayMax);
	m_Timer = (div == 0 ? 0 : GeneralUtils::GenerateRandomNumber<int>(0, div)) + info.wanderDelayMin; //set a random timer to stay put.

	const float radius = info.wanderRadius * sqrt(static_cast<double>(GeneralUtils::GenerateRandomNumber<float>(0, 1))); //our wander radius + a bit of random range
	const float theta = ((static_cast<double>(GeneralUtils::GenerateRandomNumber<float>(0, 1)) * 2 * PI));

	const NiPoint3 delta =
	{
		radius * cos(theta),
		0,
		radius * sin(theta)
	};

	auto destination = m_StartPosition + delta;

	if (dpWorld::Instance().IsLoaded()) {
		destination.y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(destination);
	}

	if (Vector3::DistanceSquared(destination, m_MovementAI->GetCurrentPosition()) < 2 * 2) {
		m_MovementAI->Stop();

		return;
	}

	m_MovementAI->SetSpeed(m_TetherSpeed);

	m_MovementAI->SetDestination(destination);

	m_Timer += (m_MovementAI->GetCurrentPosition().x - destination.x) / m_TetherSpeed;
}

void BaseCombatAIComponent::OnAggro() {
	if (m_Target == LWOOBJID_EMPTY) return;

	auto* target = GetTargetEntity();

	if (target == nullptr) {
		return;
	}

	m_MovementAI->SetHaltDistance(m_AttackRadius);

	NiPoint3 targetPos = target->GetPosition();
	NiPoint3 currentPos = m_MovementAI->GetCurrentPosition();

	// If the player's position is within range, attack
	if (Vector3::DistanceSquared(currentPos, targetPos) <= m_AttackRadius * m_AttackRadius) {
		m_MovementAI->Stop();
	} else if (Vector3::DistanceSquared(m_StartPosition, targetPos) > m_HardTetherRadius * m_HardTetherRadius) //Return to spawn if we're too far
	{
		m_MovementAI->SetSpeed(m_PursuitSpeed);

		m_MovementAI->SetDestination(m_StartPosition);
	} else //Chase the player's new position
	{
		if (IsMech() && Vector3::DistanceSquared(targetPos, currentPos) > m_AttackRadius * m_AttackRadius * 3 * 3) return;

		m_MovementAI->SetSpeed(m_PursuitSpeed);

		m_MovementAI->SetDestination(targetPos);

		m_State = AiState::tether;
	}

	m_Timer += 0.5f;
}

void BaseCombatAIComponent::OnTether() {
	auto* target = GetTargetEntity();

	if (target == nullptr) {
		return;
	}

	m_MovementAI->SetHaltDistance(m_AttackRadius);

	NiPoint3 targetPos = target->GetPosition();
	NiPoint3 currentPos = m_MovementAI->ApproximateLocation();

	if (Vector3::DistanceSquared(currentPos, targetPos) <= m_AttackRadius * m_AttackRadius) {
		m_MovementAI->Stop();
	} else if (Vector3::DistanceSquared(m_StartPosition, targetPos) > m_HardTetherRadius * m_HardTetherRadius) //Return to spawn if we're too far
	{
		m_MovementAI->SetSpeed(m_PursuitSpeed);

		m_MovementAI->SetDestination(m_StartPosition);

		m_State = AiState::aggro;
	} else {
		if (IsMech() && Vector3::DistanceSquared(targetPos, currentPos) > m_AttackRadius * m_AttackRadius * 3 * 3) return;

		m_MovementAI->SetSpeed(m_PursuitSpeed);

		m_MovementAI->SetDestination(targetPos);
	}

	m_Timer += 0.5f;
}

bool BaseCombatAIComponent::GetStunned() const {
	return m_Stunned;
}

void BaseCombatAIComponent::SetStunned(const bool value) {
	m_Stunned = value;
}

bool BaseCombatAIComponent::GetStunImmune() const {
	return m_StunImmune;
}

void BaseCombatAIComponent::SetStunImmune(bool value) {
	m_StunImmune = value;
}

float BaseCombatAIComponent::GetTetherSpeed() const {
	return m_TetherSpeed;
}

void BaseCombatAIComponent::SetTetherSpeed(float value) {
	m_TetherSpeed = value;
}

void BaseCombatAIComponent::Stun(const float time) {
	if (m_StunImmune || m_StunTime > time) {
		return;
	}

	m_StunTime = time;

	m_Stunned = true;
}

float BaseCombatAIComponent::GetAggroRadius() const {
	return m_AggroRadius;
}

void BaseCombatAIComponent::SetAggroRadius(const float value) {
	m_AggroRadius = value;
}

void BaseCombatAIComponent::LookAt(const NiPoint3& point) {
	if (m_Stunned) {
		return;
	}

	m_Parent->SetRotation(NiQuaternion::LookAt(m_Parent->GetPosition(), point));
}

void BaseCombatAIComponent::SetDisabled(bool value) {
	m_Disabled = value;
}

bool BaseCombatAIComponent::GetDistabled() const {
	return m_Disabled;
}

void BaseCombatAIComponent::Sleep() {
	m_dpEntity->SetSleeping(true);
	m_dpEntityEnemy->SetSleeping(true);
}

void BaseCombatAIComponent::Wake() {
	m_dpEntity->SetSleeping(false);
	m_dpEntityEnemy->SetSleeping(false);
}
