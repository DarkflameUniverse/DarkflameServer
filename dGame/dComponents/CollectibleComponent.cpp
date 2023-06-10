#include "CollectibleComponent.h"

#include "Entity.h"

CollectibleComponent::CollectibleComponent(Entity* parent) : Component(parent) {
	
}

void CollectibleComponent::Startup() {
	m_CollectibleId = GetParentEntity()->GetVarAs<int32_t>(u"collectible_id");
}
