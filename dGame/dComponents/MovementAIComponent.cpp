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
#include "dZoneManager.h"

#include "CDComponentsRegistryTable.h"
#include "CDPhysicsComponentTable.h"

#include "dNavMesh.h"

namespace {
	/**
	 * Cache of all lots and their respective speeds
	 */
	std::map<LOT, float> m_PhysicsSpeedCache;
}

MovementAIComponent::MovementAIComponent(Entity* parent, MovementAIInfo info) : Component(parent) {
	m_Info = info;
	m_AtFinalWaypoint = true;

	m_BaseCombatAI = nullptr;

	m_BaseCombatAI = m_Parent->GetComponent<BaseCombatAIComponent>();

	//Try and fix the insane values:
	if (m_Info.wanderRadius > 5.0f) m_Info.wanderRadius *= 0.5f;
	if (m_Info.wanderRadius > 8.0f) m_Info.wanderRadius = 8.0f;
	if (m_Info.wanderSpeed > 0.5f) m_Info.wanderSpeed *= 0.5f;

	m_BaseSpeed = GetBaseSpeed(m_Parent->GetLOT());

	m_NextWaypoint = m_Parent->GetPosition();
	m_Acceleration = 0.4f;
	m_PullingToPoint = false;
	m_PullPoint = NiPoint3Constant::ZERO;
	m_HaltDistance = 0;
	m_TimeToTravel = 0;
	m_TimeTravelled = 0;
	m_CurrentSpeed = 0;
	m_MaxSpeed = 0;
	m_LockRotation = false;
}

void MovementAIComponent::Update(const float deltaTime) {
	if (m_PullingToPoint) {
		const auto source = GetCurrentWaypoint();

		const auto speed = deltaTime * 2.5f;

		NiPoint3 velocity = (m_PullPoint - source) * speed;

		SetPosition(source + velocity);

		if (Vector3::DistanceSquared(m_Parent->GetPosition(), m_PullPoint) < std::pow(2, 2)) {
			m_PullingToPoint = false;
		}

		return;
	}

	// Are we done?
	if (AtFinalWaypoint()) return;

	if (m_HaltDistance > 0) {
		// Prevent us from hugging the target
		if (Vector3::DistanceSquared(ApproximateLocation(), GetDestination()) < std::pow(m_HaltDistance, 2)) {
			Stop();
			return;
		}
	}

	m_TimeTravelled += deltaTime;
	if (m_TimeTravelled < m_TimeToTravel) return;
	m_TimeTravelled = 0.0f;

	const auto source = GetCurrentWaypoint();

	SetPosition(source);

	NiPoint3 velocity = NiPoint3Constant::ZERO;

	if (m_Acceleration > 0 && m_BaseSpeed > 0 && AdvanceWaypointIndex()) // Do we have another waypoint to seek?
	{
		m_NextWaypoint = GetCurrentWaypoint();

		if (m_NextWaypoint == source) {
			m_TimeToTravel = 0.0f;

			goto nextAction;
		}

		if (m_CurrentSpeed < m_MaxSpeed) {
			m_CurrentSpeed += m_Acceleration;
		}

		if (m_CurrentSpeed > m_MaxSpeed) {
			m_CurrentSpeed = m_MaxSpeed;
		}

		const auto speed = m_CurrentSpeed * m_BaseSpeed; // scale speed based on base speed

		const auto delta = m_NextWaypoint - source;

		// Normalize the vector
		const auto length = delta.Length();
		if (length > 0) {
			velocity = (delta / length) * speed;
		}

		// Calclute the time it will take to reach the next waypoint with the current speed
		m_TimeTravelled = 0.0f;
		m_TimeToTravel = length / speed;

		SetRotation(NiQuaternion::LookAt(source, m_NextWaypoint));
	} else {
		// Check if there are more waypoints in the queue, if so set our next destination to the next waypoint
		if (m_CurrentPath.empty()) {
			Stop();

			return;
		}
		SetDestination(m_CurrentPath.top());

		m_CurrentPath.pop();
	}

nextAction:

	SetVelocity(velocity);

	Game::entityManager->SerializeEntity(m_Parent);
}

const MovementAIInfo& MovementAIComponent::GetInfo() const {
	return m_Info;
}

bool MovementAIComponent::AdvanceWaypointIndex() {
	if (m_PathIndex >= m_InterpolatedWaypoints.size()) {
		return false;
	}

	m_PathIndex++;

	return true;
}

NiPoint3 MovementAIComponent::GetCurrentWaypoint() const {
	return m_PathIndex >= m_InterpolatedWaypoints.size() ? m_Parent->GetPosition() : m_InterpolatedWaypoints[m_PathIndex];
}

NiPoint3 MovementAIComponent::ApproximateLocation() const {
	auto source = m_Parent->GetPosition();

	if (AtFinalWaypoint()) return source;

	auto destination = m_NextWaypoint;

	auto percentageToWaypoint = m_TimeToTravel > 0 ? m_TimeTravelled / m_TimeToTravel : 0;

	auto approximation = source + ((destination - source) * percentageToWaypoint);

	if (dpWorld::IsLoaded()) {
		approximation.y = dpWorld::GetNavMesh()->GetHeightAtPoint(approximation);
	}

	return approximation;
}

bool MovementAIComponent::Warp(const NiPoint3& point) {
	Stop();

	NiPoint3 destination = point;

	if (dpWorld::IsLoaded()) {
		destination.y = dpWorld::GetNavMesh()->GetHeightAtPoint(point);

		if (std::abs(destination.y - point.y) > 3) {
			return false;
		}
	}

	SetPosition(destination);

	Game::entityManager->SerializeEntity(m_Parent);

	return true;
}

void MovementAIComponent::Stop() {
	if (AtFinalWaypoint()) return;

	SetPosition(ApproximateLocation());

	SetVelocity(NiPoint3Constant::ZERO);

	m_TimeToTravel = 0;
	m_TimeTravelled = 0;

	m_AtFinalWaypoint = true;

	m_InterpolatedWaypoints.clear();
	while (!m_CurrentPath.empty()) m_CurrentPath.pop();

	m_PathIndex = 0;

	m_CurrentSpeed = 0;

	Game::entityManager->SerializeEntity(m_Parent);
}

void MovementAIComponent::PullToPoint(const NiPoint3& point) {
	Stop();

	m_PullingToPoint = true;
	m_PullPoint = point;
}

void MovementAIComponent::SetPath(std::vector<NiPoint3> path) {
	if (path.empty()) return;
	std::for_each(path.rbegin(), path.rend() - 1, [this](const NiPoint3& point) {
		this->m_CurrentPath.push(point);
		});

	SetDestination(path.front());
}

float MovementAIComponent::GetBaseSpeed(LOT lot) {
	// Check if the lot is in the cache
	const auto& it = m_PhysicsSpeedCache.find(lot);

	if (it != m_PhysicsSpeedCache.end()) {
		return it->second;
	}

	CDComponentsRegistryTable* componentRegistryTable = CDClientManager::Instance().GetTable<CDComponentsRegistryTable>();
	CDPhysicsComponentTable* physicsComponentTable = CDClientManager::Instance().GetTable<CDPhysicsComponentTable>();

	int32_t componentID;
	CDPhysicsComponent* physicsComponent = nullptr;

	componentID = componentRegistryTable->GetByIDAndType(lot, eReplicaComponentType::CONTROLLABLE_PHYSICS, -1);

	if (componentID == -1) {
		componentID = componentRegistryTable->GetByIDAndType(lot, eReplicaComponentType::SIMPLE_PHYSICS, -1);
	}

	physicsComponent = physicsComponentTable->GetByID(componentID);

	// Client defaults speed to 10 and if the speed is also null in the table, it defaults to 10.
	float speed = physicsComponent != nullptr ? physicsComponent->speed : 10.0f;

	float delta = fabs(speed) - 1.0f;

	if (delta <= std::numeric_limits<float>::epsilon()) speed = 10.0f;

	m_PhysicsSpeedCache[lot] = speed;

	return speed;
}

void MovementAIComponent::SetPosition(const NiPoint3& value) {
	m_Parent->SetPosition(value);
}

void MovementAIComponent::SetRotation(const NiQuaternion& value) {
	if (!m_LockRotation) m_Parent->SetRotation(value);
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

void MovementAIComponent::SetDestination(const NiPoint3& destination) {
	if (m_PullingToPoint) return;

	const auto location = ApproximateLocation();

	if (!AtFinalWaypoint()) {
		SetPosition(location);
	}

	std::vector<NiPoint3> computedPath;
	if (dpWorld::IsLoaded()) {
		computedPath = dpWorld::GetNavMesh()->GetPath(m_Parent->GetPosition(), destination, m_Info.wanderSpeed);
	}

	// Somehow failed
	if (computedPath.empty()) {
		// Than take 10 points between the current position and the destination and make that the path

		auto start = location;

		auto delta = destination - start;

		auto step = delta / 10.0f;

		for (int i = 0; i < 10; i++) {
			// TODO: Replace this with += when the NiPoint3::operator+= is fixed
			start = start + step;

			computedPath.push_back(start);
		}
	}

	m_InterpolatedWaypoints.clear();

	// Simply path
	for (auto& point : computedPath) {
		if (dpWorld::IsLoaded()) {
			point.y = dpWorld::GetNavMesh()->GetHeightAtPoint(point);
		}

		m_InterpolatedWaypoints.push_back(point);
	}

	m_PathIndex = 0;

	m_TimeTravelled = 0;
	m_TimeToTravel = 0;

	m_AtFinalWaypoint = false;
}

NiPoint3 MovementAIComponent::GetDestination() const {
	return m_InterpolatedWaypoints.empty() ? m_Parent->GetPosition() : m_InterpolatedWaypoints.back();
}

void MovementAIComponent::SetMaxSpeed(const float value) {
	if (value == m_MaxSpeed) return;
	m_MaxSpeed = value;
	m_Acceleration = value / 5;
}
