#include "Component.h"
#include "DluAssert.h"

Component::Component(Entity* owningEntity) {
	DluAssert(owningEntity != nullptr);
	m_ParentEntity = owningEntity;
}
