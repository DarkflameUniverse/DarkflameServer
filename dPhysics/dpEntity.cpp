#include "dpEntity.h"
#include "dpShapeSphere.h"
#include "dpShapeBox.h"
#include "dpGrid.h"

#include <iostream>

dpEntity::dpEntity(const LWOOBJID& objectID, dpShapeType shapeType, bool isStatic) {
	m_ObjectID = objectID;
	m_IsStatic = isStatic;
	m_CollisionShape = nullptr;
	m_Scale = 1.0f;
	m_CollisionGroup = COLLISION_GROUP_ALL;

	switch (shapeType) {
	case dpShapeType::Sphere:
		m_CollisionShape = new dpShapeSphere(this, 1.0f);
		break;

	case dpShapeType::Box:
		m_CollisionShape = new dpShapeBox(this, 1.0f, 1.0f, 1.0f);
		break;

	default:
		std::cout << "No shape for shapeType: " << (int)shapeType << std::endl;
	}
}

dpEntity::dpEntity(const LWOOBJID& objectID, NiPoint3 boxDimensions, bool isStatic) {
	m_ObjectID = objectID;
	m_IsStatic = isStatic;
	m_CollisionShape = nullptr;
	m_Scale = 1.0f;
	m_CollisionGroup = COLLISION_GROUP_ALL;

	m_CollisionShape = new dpShapeBox(this, boxDimensions.x, boxDimensions.y, boxDimensions.z);
	if (boxDimensions.x > 100.0f) m_IsGargantuan = true;
}

dpEntity::dpEntity(const LWOOBJID& objectID, float width, float height, float depth, bool isStatic) {
	m_ObjectID = objectID;
	m_IsStatic = isStatic;
	m_CollisionShape = nullptr;
	m_Scale = 1.0f;
	m_CollisionGroup = COLLISION_GROUP_ALL;

	m_CollisionShape = new dpShapeBox(this, width, height, depth);
	if (width > 100.0f) m_IsGargantuan = true;
}

dpEntity::dpEntity(const LWOOBJID& objectID, float radius, bool isStatic) {
	m_ObjectID = objectID;
	m_IsStatic = isStatic;
	m_CollisionShape = nullptr;
	m_Scale = 1.0f;
	m_CollisionGroup = COLLISION_GROUP_ALL;

	m_CollisionShape = new dpShapeSphere(this, radius);
	if (radius > 200.0f) m_IsGargantuan = true;
}

dpEntity::~dpEntity() {
	delete m_CollisionShape;
	m_CollisionShape = nullptr;
}

void dpEntity::Update(float deltaTime) {
	m_NewObjects.clear();
	m_RemovedObjects.clear();

	if (m_IsStatic) return;
	//m_Position = m_Position + (m_Velocity * deltaTime);
}

void dpEntity::CheckCollision(dpEntity* other) {
	if (!m_CollisionShape) return;

	if ((m_CollisionGroup & other->m_CollisionGroup) & (~COLLISION_GROUP_DYNAMIC)) {
		return;
	}

	bool wasFound = (m_CurrentlyCollidingObjects.find(other->GetObjectID()) != m_CurrentlyCollidingObjects.end());

	bool isColliding = m_CollisionShape->IsColliding(other->GetShape());

	if (isColliding && !wasFound) {
		m_CurrentlyCollidingObjects.emplace(other->GetObjectID(), other);
		m_NewObjects.push_back(other);

		//if (m_CollisionShape->GetShapeType() == dpShapeType::Sphere && other->GetShape()->GetShapeType() == dpShapeType::Sphere)
			//std::cout << "started sphere col at: " << other->GetPosition().x << ", " << other->GetPosition().y << ", " << other->GetPosition().z << std::endl;
	} else if (!isColliding && wasFound) {
		m_CurrentlyCollidingObjects.erase(other->GetObjectID());
		m_RemovedObjects.push_back(other);

		//if (m_CollisionShape->GetShapeType() == dpShapeType::Sphere && other->GetShape()->GetShapeType() == dpShapeType::Sphere)
		//	std::cout << "stopped sphere col at: " << other->GetPosition().x << ", " << other->GetPosition().y << ", " << other->GetPosition().z << std::endl;
	}
}

void dpEntity::SetPosition(const NiPoint3& newPos) {
	if (!m_CollisionShape) return;

	//Update the grid if needed:
	if (m_Grid) m_Grid->Move(this, newPos.x, newPos.z);

	//If we're a box, we need to first undo the previous position, otherwise things get screwy:
	if (m_CollisionShape->GetShapeType() == dpShapeType::Box) {
		auto box = static_cast<dpShapeBox*>(m_CollisionShape);

		if (m_Position != NiPoint3()) {
			box->SetPosition(NiPoint3(-m_Position.x, -m_Position.y, -m_Position.z));
		}

		box->SetPosition(newPos);
	}

	m_Position = newPos;
}

void dpEntity::SetRotation(const NiQuaternion& newRot) {
	m_Rotation = newRot;

	if (m_CollisionShape->GetShapeType() == dpShapeType::Box) {
		auto box = static_cast<dpShapeBox*>(m_CollisionShape);
		box->SetRotation(newRot);
	}
}

void dpEntity::SetScale(float newScale) {
	m_Scale = newScale;

	if (m_CollisionShape->GetShapeType() == dpShapeType::Box) {
		auto box = static_cast<dpShapeBox*>(m_CollisionShape);
		box->SetScale(newScale);
	}
}

void dpEntity::SetVelocity(const NiPoint3& newVelocity) {
	m_Velocity = newVelocity;
}

void dpEntity::SetAngularVelocity(const NiPoint3& newAngularVelocity) {
	m_AngularVelocity = newAngularVelocity;
}

void dpEntity::SetGrid(dpGrid* grid) {
	m_Grid = grid;
	m_Grid->Add(this);
}
