#include "MovementAIComponent.h"

#include <utility>
#include <cmath>

#include "ControllablePhysicsComponent.h"
#include "BaseCombatAIComponent.h"
#include "dpCommon.h"
#include "dpWorld.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"

std::map<LOT, float> MovementAIComponent::m_PhysicsSpeedCache = {};

MovementAIComponent::MovementAIComponent(Entity* parent, MovementAIInfo info) : Component(parent) {
	m_Info = std::move(info);
	m_Done = true;

	m_BaseCombatAI = nullptr;

	m_BaseCombatAI = reinterpret_cast<BaseCombatAIComponent*>(m_Parent->GetComponent(COMPONENT_TYPE_BASE_COMBAT_AI));

	//Try and fix the insane values:
	if (m_Info.wanderRadius > 5.0f) m_Info.wanderRadius = m_Info.wanderRadius * 0.5f;
	if (m_Info.wanderRadius > 8.0f) m_Info.wanderRadius = 8.0f;
	if (m_Info.wanderSpeed > 0.5f) m_Info.wanderSpeed = m_Info.wanderSpeed * 0.5f;

	m_BaseSpeed = GetBaseSpeed(m_Parent->GetLOT());

	m_NextWaypoint = GetCurrentPosition();
	m_Acceleration = 0.4f;
	m_Interrupted = false;
	m_PullPoint = {};
	m_HaltDistance = 0;
	m_Timer = 0;
	m_CurrentSpeed = 0;
	m_Speed = 0;
	m_TotalTime = 0;
	m_LockRotation = false;
}

MovementAIComponent::~MovementAIComponent() = default;

void MovementAIComponent::Update(const float deltaTime) {
	if (m_Interrupted) {
		const auto source = GetCurrentWaypoint();

		const auto speed = deltaTime * 2.5f;

		NiPoint3 velocity;

		velocity.x = (m_PullPoint.x - source.x) * speed;
		velocity.y = (m_PullPoint.y - source.y) * speed;
		velocity.z = (m_PullPoint.z - source.z) * speed;

		SetPosition(source + velocity);

		if (Vector3::DistanceSquared(GetCurrentPosition(), m_PullPoint) < 2 * 2) {
			m_Interrupted = false;
		}

		return;
	}

	if (AtFinalWaypoint()) // Are we done?
	{
		return;
	}

	if (m_HaltDistance > 0) {
		if (Vector3::DistanceSquared(ApproximateLocation(), GetDestination()) < m_HaltDistance * m_HaltDistance) // Prevent us from hugging the target
		{
			Stop();

			return;
		}
	}

	if (m_Timer > 0) {
		m_Timer -= deltaTime;

		if (m_Timer > 0) {
			return;
		}

		m_Timer = 0;
	}

	const auto source = GetCurrentWaypoint();

	SetPosition(source);

	NiPoint3 velocity = NiPoint3::ZERO;

	if (AdvanceWaypointIndex()) // Do we have another waypoint to seek?
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

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

const MovementAIInfo& MovementAIComponent::GetInfo() const {
	return m_Info;
}

bool MovementAIComponent::AdvanceWaypointIndex() {
	if (m_PathIndex >= m_CurrentPath.size()) {
		return false;
	}

	m_PathIndex++;

	return true;
}

NiPoint3 MovementAIComponent::GetCurrentWaypoint() const {
	if (m_PathIndex >= m_CurrentPath.size()) {
		return GetCurrentPosition();
	}

	return m_CurrentPath[m_PathIndex];
}

NiPoint3 MovementAIComponent::GetNextWaypoint() const {
	return m_NextWaypoint;
}

NiPoint3 MovementAIComponent::GetCurrentPosition() const {
	return m_Parent->GetPosition();
}

NiPoint3 MovementAIComponent::ApproximateLocation() const {
	auto source = GetCurrentPosition();

	if (m_Done) {
		return source;
	}

	auto destination = m_NextWaypoint;

	auto factor = m_TotalTime > 0 ? (m_TotalTime - m_Timer) / m_TotalTime : 0;

	auto x = source.x + factor * (destination.x - source.x);
	auto y = source.y + factor * (destination.y - source.y);
	auto z = source.z + factor * (destination.z - source.z);

	NiPoint3 approximation = NiPoint3(x, y, z);

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

	EntityManager::Instance()->SerializeEntity(m_Parent);

	return true;
}

float MovementAIComponent::GetTimer() const {
	return m_Timer;
}

bool MovementAIComponent::AtFinalWaypoint() const {
	return m_Done;
}

void MovementAIComponent::Stop() {
	if (m_Done) {
		return;
	}

	SetPosition(ApproximateLocation());

	SetVelocity(NiPoint3::ZERO);

	m_TotalTime = m_Timer = 0;

	m_Done = true;

	m_CurrentPath = {};

	m_PathIndex = 0;

	m_CurrentSpeed = 0;

	EntityManager::Instance()->SerializeEntity(m_Parent);
}

void MovementAIComponent::PullToPoint(const NiPoint3& point) {
	Stop();

	m_Interrupted = true;
	m_PullPoint = point;
}

void MovementAIComponent::SetPath(std::vector<NiPoint3> path) {
	std::reverse(path.begin(), path.end());

	for (const auto& point : path) {
		m_Queue.push(point);
	}

	SetDestination(m_Queue.top());

	m_Queue.pop();
}

float MovementAIComponent::GetBaseSpeed(LOT lot) {
	// Check if the lot is in the cache
	const auto& it = m_PhysicsSpeedCache.find(lot);

	if (it != m_PhysicsSpeedCache.end()) {
		return it->second;
	}

	CDComponentsRegistryTable* componentRegistryTable = CDClientManager::Instance()->GetTable<CDComponentsRegistryTable>("ComponentsRegistry");
	CDPhysicsComponentTable* physicsComponentTable = CDClientManager::Instance()->GetTable<CDPhysicsComponentTable>("PhysicsComponent");

	int32_t componentID;
	CDPhysicsComponent* physicsComponent = nullptr;

	componentID = componentRegistryTable->GetByIDAndType(lot, COMPONENT_TYPE_CONTROLLABLE_PHYSICS, -1);

	if (componentID != -1) {
		physicsComponent = physicsComponentTable->GetByID(componentID);

		goto foundComponent;
	}

	componentID = componentRegistryTable->GetByIDAndType(lot, COMPONENT_TYPE_SIMPLE_PHYSICS, -1);

	if (componentID != -1) {
		physicsComponent = physicsComponentTable->GetByID(componentID);

		goto foundComponent;
	}

foundComponent:

	float speed;

	if (physicsComponent == nullptr) {
		speed = 8;
	} else {
		speed = physicsComponent->speed;
	}

	m_PhysicsSpeedCache[lot] = speed;

	return speed;
}

void MovementAIComponent::SetPosition(const NiPoint3& value) {
	auto* controllablePhysicsComponent = m_Parent->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent != nullptr) {
		controllablePhysicsComponent->SetPosition(value);

		return;
	}

	auto* simplePhysicsComponent = m_Parent->GetComponent<SimplePhysicsComponent>();

	if (simplePhysicsComponent != nullptr) {
		simplePhysicsComponent->SetPosition(value);
	}
}

void MovementAIComponent::SetRotation(const NiQuaternion& value) {
	if (m_LockRotation) {
		return;
	}

	auto* controllablePhysicsComponent = m_Parent->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent != nullptr) {
		controllablePhysicsComponent->SetRotation(value);

		return;
	}

	auto* simplePhysicsComponent = m_Parent->GetComponent<SimplePhysicsComponent>();

	if (simplePhysicsComponent != nullptr) {
		simplePhysicsComponent->SetRotation(value);
	}
}

void MovementAIComponent::SetVelocity(const NiPoint3& value) {
	auto* controllablePhysicsComponent = m_Parent->GetComponent<ControllablePhysicsComponent>();

	if (controllablePhysicsComponent != nullptr) {
		controllablePhysicsComponent->SetVelocity(value);

		return;
	}

	auto* simplePhysicsComponent = m_Parent->GetComponent<SimplePhysicsComponent>();

	if (simplePhysicsComponent != nullptr) {
		simplePhysicsComponent->SetVelocity(value);
	}
}

void MovementAIComponent::SetDestination(const NiPoint3& value) {
	if (m_Interrupted) {
		return;
	}

	/*if (Vector3::DistanceSquared(value, GetDestination()) < 2 * 2)
	{
		return;
	}*/

	const auto location = ApproximateLocation();

	if (!AtFinalWaypoint()) {
		SetPosition(location);
	}

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

	if (computedPath.empty()) // Somehow failed
	{
		return;
	}

	m_CurrentPath.clear();

	m_CurrentPath.push_back(location);

	// Simply path
	for (auto point : computedPath) {
		if (dpWorld::Instance().IsLoaded()) {
			point.y = dpWorld::Instance().GetNavMesh()->GetHeightAtPoint(point);
		}

		m_CurrentPath.push_back(point);
	}

	m_CurrentPath.push_back(computedPath[computedPath.size() - 1]);

	m_PathIndex = 0;

	m_TotalTime = m_Timer = 0;

	m_Done = false;
}

NiPoint3 MovementAIComponent::GetDestination() const {
	if (m_CurrentPath.empty()) {
		return GetCurrentPosition();
	}

	return m_CurrentPath[m_CurrentPath.size() - 1];
}

void MovementAIComponent::SetSpeed(const float value) {
	m_Speed = value;
	m_Acceleration = value / 5;
}

float MovementAIComponent::GetSpeed() const {
	return m_Speed;
}

void MovementAIComponent::SetAcceleration(const float value) {
	m_Acceleration = value;
}

float MovementAIComponent::GetAcceleration() const {
	return m_Acceleration;
}

void MovementAIComponent::SetHaltDistance(const float value) {
	m_HaltDistance = value;
}

float MovementAIComponent::GetHaltDistance() const {
	return m_HaltDistance;
}

void MovementAIComponent::SetCurrentSpeed(float value) {
	m_CurrentSpeed = value;
}

float MovementAIComponent::GetCurrentSpeed() const {
	return m_CurrentSpeed;
}

void MovementAIComponent::SetLockRotation(bool value) {
	m_LockRotation = value;
}

bool MovementAIComponent::GetLockRotation() const {
	return m_LockRotation;
}
