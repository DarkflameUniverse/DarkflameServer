#include "dpShapeSphere.h"
#include "dpCollisionChecks.h"
#include <iostream>

dpShapeSphere::dpShapeSphere(dpEntity* parentEntity, float radius) :
	dpShapeBase(parentEntity),
	m_Radius(radius) {
	m_ShapeType = dpShapeType::Sphere;
}

dpShapeSphere::~dpShapeSphere() {
}

bool dpShapeSphere::IsColliding(dpShapeBase* other) {
	if (!other) return false;

	switch (other->GetShapeType()) {
	case dpShapeType::Sphere:
		return dpCollisionChecks::CheckSpheres(m_ParentEntity, other->GetParentEntity());

	case dpShapeType::Box:
		return dpCollisionChecks::CheckSphereBox(m_ParentEntity, other->GetParentEntity());

	default:
		std::cout << "No collision detection for: " << (int)m_ShapeType << "-to-" << (int)other->GetShapeType() << " collision!" << std::endl;
	}

	return false;
}
