#include "dpShapeBase.h"
#include "dpEntity.h"
#include <iostream>

dpShapeBase::dpShapeBase(dpEntity* parentEntity) :
	m_ParentEntity(parentEntity) {
}

dpShapeBase::~dpShapeBase() {
}

bool dpShapeBase::IsColliding(dpShapeBase* other) {
	std::cout << "Base shapes do not have any *shape* to them, and thus cannot be overlapping." << std::endl;
	std::cout << "You should be using a shape class inherited from this base class." << std::endl;
	return false;
}
