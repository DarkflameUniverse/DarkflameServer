#include "dpShapeSphere.h"
#include "dpCollisionChecks.h"
#include "Game.h"
#include "Logger.h"
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
		LOG("No collision detection for: %i-to-%i collision!", static_cast<int32_t>(m_ShapeType), static_cast<int32_t>(other->GetShapeType()));
	}

	return false;
}
