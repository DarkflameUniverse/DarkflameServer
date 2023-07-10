#include "MovementAIComponent.h"

#include <utility>
#include <cmath>

#include "ControllablePhysicsComponent.h"
#include "BaseCombatAIComponent.h"
#include "dpCommon.h"
#include "dpWorld.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"
#include "CDClientManager.h"
#include "Game.h"
#include "dLogger.h"
#include "SimplePhysicsComponent.h"
#include "ControllablePhysicsComponent.h"

#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"
#include "CDMovementAIComponentTable.h"
#include "Entity.h"
#include "BaseCombatAIComponent.h"

std::map<LOT, float> MovementAIComponent::m_PhysicsSpeedCache = {};

MovementAIComponent::MovementAIComponent(Entity* parent, int32_t componentId) : Component(parent) {
	m_ComponentId = componentId;
	m_Done = true;

	m_BaseCombatAI = nullptr;

	m_Acceleration = 0.4f;
	m_Interrupted = false;
	m_PullPoint = NiPoint3::ZERO;
	m_HaltDistance = 0;
	m_Timer = 0;
	m_CurrentSpeed = 0;
	m_Speed = 0;
	m_TotalTime = 0;
	m_LockRotation = false;
}

void MovementAIComponent::Startup() {
	m_BaseCombatAI = m_ParentEntity->GetComponent<BaseCombatAIComponent>();
	m_NextWaypoint = GetCurrentPosition();
}

void MovementAIComponent::LoadConfigData() {
	bool useWanderDB = m_ParentEntity->GetVar<bool>(u"usewanderdb");

	if (useWanderDB) return;
	const auto wanderOverride = m_ParentEntity->GetVarAs<float>(u"wanderRadius");

	if (wanderOverride != 0.0f) m_Info.wanderRadius = wanderOverride;
}

void MovementAIComponent::LoadTemplateData() {
	m_BaseSpeed = GetBaseSpeed(m_ParentEntity->GetLOT());
	if (m_ComponentId == -1) return;
	auto* movementAiComponentTable = CDClientManager::Instance().GetTable<CDMovementAIComponentTable>();
	auto movementEntries = movementAiComponentTable->Query([this](CDMovementAIComponent entry) {return (entry.id == this->m_ComponentId); });
	if (movementEntries.empty()) return;
	auto movementEntry = movementEntries.at(0);
	MovementAIInfo moveInfo{};

	moveInfo.movementType = movementEntry.MovementType;
	moveInfo.wanderChance = movementEntry.WanderChance;
	moveInfo.wanderRadius = movementEntry.WanderRadius;
	moveInfo.wanderSpeed = movementEntry.WanderSpeed;
	moveInfo.wanderDelayMax = movementEntry.WanderDelayMax;
	moveInfo.wanderDelayMin = movementEntry.WanderDelayMin;

	this->SetMoveInfo(moveInfo);
}

void MovementAIComponent::SetMoveInfo(const MovementAIInfo& info) {
	m_Info = info;

	//Try and fix the insane values:
	if (m_Info.wanderRadius > 5.0f) m_Info.wanderRadius *= 0.5f;
	if (m_Info.wanderRadius > 8.0f) m_Info.wanderRadius = 8.0f;
	if (m_Info.wanderSpeed > 0.5f) m_Info.wanderSpeed *= 0.5f;
}

void MovementAIComponent::Update(const float deltaTime) {
	if (m_Interrupted) {
		const auto source = GetCurrentWaypoint();

		const auto speed = deltaTime * 2.5f;

		NiPoint3 velocity(
			(m_PullPoint.x - source.x) * speed,
			(m_PullPoint.y - source.y) * speed,
			(m_PullPoint.z - source.z) * speed
		);

		SetPosition(source + velocity);

		if (Vector3::DistanceSquared(GetCurrentPosition(), m_PullPoint) < 2 * 2) {
			m_Interrupted = false;
		}

		return;
	}

	// Are we done?
	if (AtFinalWaypoint()) return;

	if (m_HaltDistance > 0) {
		// Prevent us from hugging the target
		if (Vector3::DistanceSquared(ApproximateLocation(), GetDestination()) < m_HaltDistance * m_HaltDistance) {
			Stop();

			return;
		}
	}

	if (m_Timer > 0.0f) {
		m_Timer -= deltaTime;

		if (m_Timer > 0.0f) return;

		m_Timer = 0.0f;
	}

	const auto source = GetCurrentWaypoint();

	SetPosition(source);

	NiPoint3 velocity = NiPoint3::ZERO;

	if (m_Acceleration > 0 && m_BaseSpeed > 0 && AdvanceWaypointIndex()) // Do we have another waypoint to seek?
	{
		m_NextWaypoint = GetCurrentWaypoint();

		if (m_NextWaypoint == source) {
			m_Timer = 0;

			goto nextAction;
		}

		if (m_CurrentSpeed < m_Speed) {
			m_CurrentSpeed += m_Acceleration;
		}

		if (m_CurrentSpeed > m_Speed) {
			m_CurrentSpeed = m_Speed;
		}

		const auto speed = m_CurrentSpeed * m_BaseSpeed;

		const auto delta = m_NextWaypoint - source;

		// Normalize the vector
		const auto length = sqrtf(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);

		if (length > 0) {
			velocity.x = (delta.x / length) * speed;
			velocity.y = (delta.y / length) * speed;
			velocity.z = (delta.z / length) * speed;
		}

		// Calclute the time it will take to reach the next waypoint with the current speed
		m_TotalTime = m_Timer = length / speed;

		SetRotation(NiQuaternion::LookAt(source, m_NextWaypoint));
	} else {
		// Check if there are more waypoints in the queue, if so set our next destination to the next waypoint
		if (!m_Queue.empty()) {
			SetDestination(m_Queue.top());

			m_Queue.pop();
		} else {
			// We have reached our final waypoint
			Stop();

			return;
		}
	}

nextAction:

	SetVelocity(velocity);

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);
}

bool MovementAIComponent::AdvanceWaypointIndex() {
	if (m_PathIndex >= m_CurrentPath.size()) {
		return false;
	}

	m_PathIndex++;

	return true;
}

NiPoint3 MovementAIComponent::GetCurrentWaypoint() const {
	return m_PathIndex >= m_CurrentPath.size() ? GetCurrentPosition() : m_CurrentPath[m_PathIndex];
}

NiPoint3 MovementAIComponent::ApproximateLocation() const {
	auto source = GetCurrentPosition();

	if (m_Done) return source;

	auto destination = m_NextWaypoint;

	auto factor = m_TotalTime > 0.0f ? (m_TotalTime - m_Timer) / m_TotalTime : 0.0f;

	NiPoint3 approximation = NiPoint3(
		source.x + factor * (destination.x - source.x),
		source.y + factor * (destination.y - source.y),
		source.z + factor * (destination.z - source.z)
	);

	if (dpWorld::Instance().IsLoaded()) {
		approximation.y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(approximation);
	}

	return approximation;
}

bool MovementAIComponent::Warp(const NiPoint3& point) {
	Stop();

	NiPoint3 destination = point;

	if (dpWorld::Instance().IsLoaded()) {
		destination.y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(point);

		if (std::abs(destination.y - point.y) > 3) {
			return false;
		}
	}

	SetPosition(destination);

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);

	return true;
}

void MovementAIComponent::Stop() {
	if (m_Done) return;

	SetPosition(ApproximateLocation());

	SetVelocity(NiPoint3::ZERO);

	m_TotalTime = 0.0f;
	m_Timer = 0.0f;

	m_Done = true;

	m_CurrentPath.clear();

	m_PathIndex = 0;

	m_CurrentSpeed = 0.0f;

	EntityManager::Instance()->SerializeEntity(m_ParentEntity);
}

void MovementAIComponent::PullToPoint(const NiPoint3& point) {
	Stop();

	m_Interrupted = true;
	m_PullPoint = point;
}

void MovementAIComponent::SetPath(const std::vector<NiPoint3>& path) {
	for (auto itr = path.rbegin(); itr != path.rend(); ++itr) {
		m_Queue.push(*itr);
	}

	SetDestination(m_Queue.top());

	m_Queue.pop();
}

float MovementAIComponent::GetBaseSpeed(const LOT lot) {
	// Check if the lot is in the cache
	const auto& it = m_PhysicsSpeedCache.find(lot);

	if (it != m_PhysicsSpeedCache.end()) {
		return it->second;
	}

	auto* componentRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	auto* physicsComponentTable = CDClientManager::Instance().GetTable<CDPhysicsComponentTable>();

	int32_t componentID;
	CDPhysicsComponent* physicsComponent = nullptr;

	componentID = componentRegistryTable->GetByIDAndType(lot, eReplicaComponentType::CONTROLLABLE_PHYSICS, -1);

	if (componentID == -1) {
		componentID = componentRegistryTable->GetByIDAndType(lot, eReplicaComponentType::SIMPLE_PHYSICS, -1);
	}

	physicsComponent = physicsComponentTable->GetByID(componentID);
	// Client defaults speed to 10 and if the speed is also null in the table, it defaults to 10.
	float speed = 10.0f;

	if (physicsComponent) speed = physicsComponent->speed;

	float delta = fabs(speed) - 1.0f;

	if (delta <= std::numeric_limits<float>::epsilon()) speed = 10.0f;

	m_PhysicsSpeedCache[lot] = speed;

	return speed;
}

void MovementAIComponent::SetPosition(const NiPoint3& value) const {
	m_ParentEntity->SetPosition(value);
}

void MovementAIComponent::SetRotation(const NiQuaternion& value) const {
	if (!m_LockRotation) m_ParentEntity->SetRotation(value);
}

void MovementAIComponent::SetVelocity(const NiPoint3& value) const {
	auto* controllablePhysicsComponent = m_ParentEntity->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent) {
		controllablePhysicsComponent->SetVelocity(value);

		return;
	}

	auto* simplePhysicsComponent = m_ParentEntity->GetComponent<SimplePhysicsComponent>();

	if (simplePhysicsComponent) {
		simplePhysicsComponent->SetVelocity(value);
	}
}

void MovementAIComponent::SetDestination(const NiPoint3& value) {
	if (m_Interrupted) return;

	const auto location = ApproximateLocation();

	if (!AtFinalWaypoint()) SetPosition(location);

	std::vector<NiPoint3> computedPath;

	if (dpWorld::Instance().IsLoaded()) {
		computedPath = dpWorld::Instance().GetNavMesh()->GetPath(GetCurrentPosition(), value, m_Info.wanderSpeed);
	} else {
		// Than take 10 points between the current position and the destination and make that the path

		auto point = location;

		auto delta = value - point;

		auto step = delta / 10;

		for (int i = 0; i < 10; i++) {
			point = point + step;

			computedPath.push_back(point);
		}
	}

	// Somehow failed
	if (computedPath.empty()) return;

	m_CurrentPath.clear();

	m_CurrentPath.push_back(location);

	// Simply path
	for (auto& point : computedPath) {
		if (dpWorld::Instance().IsLoaded()) {
			point.y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(point);
		}

		m_CurrentPath.push_back(point);
	}

	m_CurrentPath.push_back(computedPath.back());

	m_PathIndex = 0;

	m_TotalTime = 0.0f;
	m_Timer = 0.0f;

	m_Done = false;
}

NiPoint3 MovementAIComponent::GetDestination() const {
	return m_CurrentPath.empty() ? GetCurrentPosition() : m_CurrentPath.back();
}

void MovementAIComponent::SetSpeed(const float value) {
	m_Speed = value;
	m_Acceleration = value / 5;
}

NiPoint3 MovementAIComponent::GetCurrentPosition() const {
	return m_ParentEntity->GetPosition();
}
